#include "webdav_server.h"
#include "logger.h"
#include "auth_manager.h"
#include "http_parser.h"
#include "file_manager.h"
#include "xml_parser.h"
#include "base64.h"
#include "mime_types.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#include <thread>
#include <mutex>
#include <algorithm>

namespace webdav {

WebDAVServer::WebDAVServer(const std::string& host, int port, const std::string& root_path)
    : host_(host), port_(port), root_path_(root_path), running_(false) {
    logger_.reset(new Logger("logs/webdav.log", Logger::Level::INFO));
    auth_manager_.reset(new AuthManager());
    http_parser_.reset(new HTTPParser(*logger_));
    file_manager_.reset(new FileManager(root_path, *logger_));
    xml_parser_.reset(new XMLParser());
    
    logger_->info("WebDAV server initializing...");
}

WebDAVServer::~WebDAVServer() {
    stop();
}

bool WebDAVServer::start() {
    logger_->info("Starting server on " + host_ + ":" + std::to_string(port_));
    
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        logger_->error("Failed to create socket: " + std::string(strerror(errno)));
        return false;
    }

    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        logger_->error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
        close(server_socket_);
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        logger_->error("Invalid address: " + host_);
        close(server_socket_);
        return false;
    }

    if (bind(server_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        logger_->error("Failed to bind socket: " + std::string(strerror(errno)));
        close(server_socket_);
        return false;
    }

    if (listen(server_socket_, SOMAXCONN) < 0) {
        logger_->error("Failed to listen on socket: " + std::string(strerror(errno)));
        close(server_socket_);
        return false;
    }

    running_ = true;
    std::thread accept_thread(&WebDAVServer::accept_connections, this);
    accept_thread.detach();

    logger_->info("Server started successfully");
    return true;
}

void WebDAVServer::stop() {
    if (running_) {
        running_ = false;
        close(server_socket_);
        
        // 等待所有工作线程结束
        {
            std::lock_guard<std::mutex> lock(threads_mutex_);
            for (auto& thread : worker_threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            worker_threads_.clear();
        }
        
        logger_->info("WebDAV server stopped");
    }
}

void WebDAVServer::accept_connections() {
    // 设置服务器 socket 为非阻塞模式
    int flags = fcntl(server_socket_, F_GETFL, 0);
    fcntl(server_socket_, F_SETFL, flags | O_NONBLOCK);
    
    fd_set read_fds;
    struct timeval tv;
    
    while (running_) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 改为 10ms 超时，让响应更快
        
        int ready = select(server_socket_ + 1, &read_fds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno != EINTR) {
                logger_->error("Select error: " + std::string(strerror(errno)));
                break;
            }
            continue;
        }
        
        if (ready == 0) {  // 超时
            continue;
        }
        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (running_) {
                logger_->error("Failed to accept connection: " + std::string(strerror(errno)));
            }
            continue;
        }
        
        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        logger_->info("New connection from " + client_ip + ":" + std::to_string(client_port));

        // 设置客户端 socket 选项
        int keepalive = 1;
        setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

        // 设置超时和缓冲区大小
        struct timeval timeout;
        timeout.tv_sec = 30;  // 30 秒
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        int buffer_size = 1024 * 1024;  // 1MB
        setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
        setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
        
        int nodelay = 1;
        setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        if (client_socket >= 0) {
            // 创建新线程
            {
                std::lock_guard<std::mutex> lock(threads_mutex_);
                
                // 清理已完成的线程
                for (auto it = worker_threads_.begin(); it != worker_threads_.end();) {
                    if (it->joinable()) {
                        it->join();
                        it = worker_threads_.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // 添加新线程
                worker_threads_.emplace_back(std::thread([this, client_socket]() {
                    handle_client(client_socket);
                }));
            }
        }
    }
}

void WebDAVServer::handle_client(int client_socket) {
    const size_t BUFFER_SIZE = 8192;  // 8KB 缓冲区
    std::vector<char> buffer(BUFFER_SIZE);
    std::vector<char> request_data;
    
    try {
        while (running_) {
            // 读取请求头
            size_t header_end = std::string::npos;
            while (header_end == std::string::npos) {
                ssize_t bytes_read = recv(client_socket, buffer.data(), buffer.size(), 0);
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        logger_->debug("Client closed connection normally");
                    } else {
                        logger_->error("Receive error: " + std::string(strerror(errno)));
                    }
                    goto cleanup;
                }
                
                request_data.insert(request_data.end(), buffer.data(), buffer.data() + bytes_read);
                std::string data(request_data.begin(), request_data.end());
                header_end = data.find("\r\n\r\n");
            }
            
            // 解析请求
            HTTPRequest request;
            if (!http_parser_->parse_request(request_data, request)) {
                // 如果有 Content-Length，继续读取请求体
                auto content_length_it = request.headers.find("Content-Length");
                if (content_length_it != request.headers.end()) {
                    size_t content_length = std::stoull(content_length_it->second);
                    size_t headers_size = header_end + 4;  // 加上 \r\n\r\n 的长度
                    size_t body_received = request_data.size() - headers_size;
                    
                    // 继续读取剩余的请求体
                    while (body_received < content_length) {
                        ssize_t bytes_read = recv(client_socket, buffer.data(), 
                                                std::min(buffer.size(), content_length - body_received), 0);
                        if (bytes_read <= 0) {
                            if (bytes_read == 0) {
                                logger_->debug("Client closed connection during body read");
                            } else {
                                logger_->error("Receive error during body read: " + std::string(strerror(errno)));
                            }
                            goto cleanup;
                        }
                        
                        request_data.insert(request_data.end(), buffer.data(), buffer.data() + bytes_read);
                        body_received += bytes_read;
                        
                        logger_->debug("Received " + std::to_string(body_received) + "/" + 
                                     std::to_string(content_length) + " bytes of body");
                    }
                    
                    // 重新解析完整的请求
                    if (!http_parser_->parse_request(request_data, request)) {
                        logger_->error("Failed to parse complete request");
                        send_error_response(client_socket, 400, "Bad Request");
                        goto cleanup;
                    }
                } else {
                    logger_->error("Failed to parse request");
                    send_error_response(client_socket, 400, "Bad Request");
                    goto cleanup;
                }
            }
            
            // 处理请求
            HTTPResponse response;
            handle_request(request, response);
            
            // 发送响应
            auto response_data = http_parser_->build_response(response);
            send(client_socket, response_data.data(), response_data.size(), 0);
            
            // 清空请求数据，准备下一个请求
            request_data.clear();
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in client thread: " + std::string(e.what()));
    } catch (...) {
        logger_->error("Unknown exception in client thread");
    }
    
cleanup:
    close(client_socket);
    logger_->debug("Client socket closed");
}

void WebDAVServer::handle_request(const HTTPRequest& request, HTTPResponse& response) {
    logger_->info("Handling request: " + std::to_string(static_cast<int>(request.method)) + 
                 " for URI: " + request.uri);
                 
    switch (request.method) {
        case HTTPMethod::OPTIONS:
            handle_options(request, response);
            break;
        case HTTPMethod::GET:
            handle_get(request, response);
            break;
        case HTTPMethod::PUT:
            handle_put(request, response);
            break;
        case HTTPMethod::DELETE:
            handle_delete(request, response);
            break;
        case HTTPMethod::MKCOL:
            handle_mkcol(request, response);
            break;
        case HTTPMethod::COPY:
            handle_copy(request, response);
            break;
        case HTTPMethod::MOVE:
            handle_move(request, response);
            break;
        case HTTPMethod::PROPFIND:
            handle_propfind(request, response);
            break;
        case HTTPMethod::PROPPATCH:
            handle_proppatch(request, response);
            break;
        case HTTPMethod::HEAD:
            handle_head(request, response);
            break;
        default:
            logger_->error("Unhandled method: " + std::to_string(static_cast<int>(request.method)));
            response.status_code = 501;
            response.status_message = "Not Implemented";
            break;
    }
}

bool WebDAVServer::authenticate(const HTTPRequest& request) {
    auto auth_header = request.headers.find("Authorization");
    if (auth_header == request.headers.end() || 
        auth_header->second.substr(0, 6) != "Basic ") {
        return false;
    }

    std::string auth_data = auth_header->second.substr(6);
    std::vector<char> decoded = Base64::decode(auth_data);
    std::string credentials(decoded.begin(), decoded.end());
    
    size_t colon_pos = credentials.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    std::string username = credentials.substr(0, colon_pos);
    std::string password = credentials.substr(colon_pos + 1);
    
    return auth_manager_->authenticate(username, password);
}

std::string WebDAVServer::decode_url(const std::string& url) {
    std::string result;
    for (size_t i = 0; i < url.length(); ++i) {
        if (url[i] == '%' && i + 2 < url.length()) {
            std::string hex = url.substr(i + 1, 2);
            char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
            result += ch;
            i += 2;
        } else {
            result += url[i];
        }
    }
    return result;
}

std::string WebDAVServer::format_http_date(time_t t) {
    char buf[100];
    struct tm* tm = gmtime(&t);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", tm);
    return std::string(buf);
}

std::string WebDAVServer::format_iso_date(time_t t) {
    char buf[100];
    struct tm* tm = gmtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm);
    return std::string(buf);
}

std::string WebDAVServer::build_xml_response(const std::string& uri, const FileInfo& info) {
    std::stringstream ss;
    ss << "  <D:response>\n"
       << "    <D:href>" << uri << "</D:href>\n"
       << "    <D:propstat>\n"
       << "      <D:prop>\n"
       << "        <D:resourcetype>";
    
    if (info.is_directory) {
        ss << "<D:collection/>";
    }
    
    ss << "</D:resourcetype>\n"
       << "        <D:getcontentlength>" << info.size << "</D:getcontentlength>\n"
       << "        <D:getlastmodified>" << format_http_date(info.modified_time) << "</D:getlastmodified>\n"
       << "        <D:creationdate>" << format_iso_date(info.created_time) << "</D:creationdate>\n"
       << "        <D:getetag>" << info.etag << "</D:getetag>\n"
       << "        <D:getcontenttype>" << MimeTypes::get_mime_type(info.name) << "</D:getcontenttype>\n"
       << "        <D:displayname>" << info.name << "</D:displayname>\n"
       << "        <D:supportedlock>\n"
       << "          <D:lockentry>\n"
       << "            <D:lockscope><D:exclusive/></D:lockscope>\n"
       << "            <D:locktype><D:write/></D:locktype>\n"
       << "          </D:lockentry>\n"
       << "        </D:supportedlock>\n";
    
    for (const auto& prop : info.properties) {
        ss << "        <" << prop.first << ">" << prop.second << "</" << prop.first << ">\n";
    }
    
    ss << "      </D:prop>\n"
       << "      <D:status>HTTP/1.1 200 OK</D:status>\n"
       << "    </D:propstat>\n"
       << "  </D:response>\n";
    
    return ss.str();
}

void WebDAVServer::send_error_response(int client_socket, int status_code, const std::string& status_message) {
    HTTPResponse response;
    response.status_code = status_code;
    response.status_message = status_message;
    response.headers["Content-Length"] = "0";
    auto response_data = http_parser_->build_response(response);
    send(client_socket, response_data.data(), response_data.size(), 0);
}

void WebDAVServer::handle_lock_request(int client_socket) {
    std::string response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/xml; charset=\"utf-8\"\r\n"
        "Lock-Token: <opaquelocktoken:" + std::to_string(time(nullptr)) + ">\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    send(client_socket, response.data(), response.size(), 0);
}

} // namespace webdav 