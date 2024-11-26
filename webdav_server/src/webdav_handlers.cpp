#include "webdav_server.h"
#include "mime_types.h"
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

namespace webdav {

void WebDAVServer::handle_options(const HTTPRequest& request, HTTPResponse& response) {
    (void)request; // 未使用的参数
    
    response.status_code = 200;
    response.status_message = "OK";
    response.headers["Allow"] = "OPTIONS, GET, HEAD, PUT, DELETE, MKCOL, COPY, MOVE, PROPFIND, PROPPATCH, LOCK, UNLOCK";
    response.headers["DAV"] = "1, 2";
    response.headers["MS-Author-Via"] = "DAV";
    response.headers["Accept-Ranges"] = "bytes";
    response.headers["Content-Length"] = "0";
    
    // Windows WebDAV 客户端需要的头
    response.headers["Connection"] = "Keep-Alive";
    response.headers["Keep-Alive"] = "timeout=5, max=100";
    response.headers["Public"] = response.headers["Allow"];
    response.headers["Server"] = "WebDAV/1.0";
    response.headers["Date"] = format_http_date(time(nullptr));
    response.headers["X-Server-Type"] = "WebDAV";
    response.headers["X-WebDAV-Status"] = "Ready";
}

void WebDAVServer::handle_get(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    FileInfo info;
    
    if (!file_manager_->get_resource_info(path, info)) {
        response.status_code = 404;
        response.status_message = "Not Found";
        return;
    }
    
    if (info.is_directory) {
        response.status_code = 301;
        response.status_message = "Moved Permanently";
        response.headers["Location"] = request.uri + "/";
        return;
    }
    
    std::vector<char> data;
    if (!file_manager_->read_file(path, data)) {
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        return;
    }
    
    response.status_code = 200;
    response.status_message = "OK";
    response.headers["Content-Type"] = MimeTypes::get_mime_type(path);
    response.headers["Content-Length"] = std::to_string(data.size());
    response.headers["ETag"] = info.etag;
    response.headers["Last-Modified"] = std::to_string(info.modified_time);
    response.body = std::move(data);
}

void WebDAVServer::handle_put(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    logger_->info("Handling PUT request for: " + path);
    
    // 检查 Content-Length
    auto content_length_it = request.headers.find("Content-Length");
    if (content_length_it == request.headers.end()) {
        logger_->error("Missing Content-Length header");
        response.status_code = 411;
        response.status_message = "Length Required";
        return;
    }
    
    // 创建临时文件
    std::string tmp_path = root_path_ + "/.tmp_" + std::to_string(time(nullptr)) + "_" + 
                          std::to_string(rand());
    int fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        logger_->error("Failed to create temp file: " + std::string(strerror(errno)));
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        return;
    }
    
    // 写入数据
    const char* data = request.body.data();
    size_t remaining = request.body.size();
    size_t total_written = 0;
    
    while (remaining > 0) {
        ssize_t written = write(fd, data + total_written, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            logger_->error("Failed to write file: " + std::string(strerror(errno)));
            close(fd);
            unlink(tmp_path.c_str());  // 删除临时文件
            response.status_code = 500;
            response.status_message = "Internal Server Error";
            return;
        }
        total_written += written;
        remaining -= written;
    }
    
    // 同步并关闭文件
    if (fsync(fd) < 0) {
        logger_->error("Failed to sync file: " + std::string(strerror(errno)));
    }
    close(fd);
    
    // 创建目标目录（如果不存在）
    std::string parent_path = path.substr(0, path.find_last_of('/'));
    if (!parent_path.empty()) {
        file_manager_->create_directory(parent_path);
    }
    
    // 移动临时文件到目标位置
    std::string dest_path = root_path_ + path;
    if (rename(tmp_path.c_str(), dest_path.c_str()) != 0) {
        logger_->error("Failed to move temp file: " + std::string(strerror(errno)));
        unlink(tmp_path.c_str());  // 删除临时文件
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        return;
    }
    
    // 检查文件是否已存在（用于决定返回状态码）
    FileInfo info;
    bool exists = file_manager_->get_resource_info(path, info);
    
    // 设置响应
    response.status_code = exists ? 204 : 201;  // No Content : Created
    response.status_message = exists ? "No Content" : "Created";
    response.headers["Content-Length"] = "0";
    
    logger_->info("File uploaded successfully: " + path + 
                 " (size: " + std::to_string(request.body.size()) + " bytes)");
}

void WebDAVServer::handle_delete(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    
    if (!file_manager_->delete_resource(path)) {
        response.status_code = 404;
        response.status_message = "Not Found";
        return;
    }
    
    response.status_code = 204;
    response.status_message = "No Content";
}

void WebDAVServer::handle_mkcol(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    
    if (!file_manager_->create_directory(path)) {
        response.status_code = 409;
        response.status_message = "Conflict";
        return;
    }
    
    response.status_code = 201;
    response.status_message = "Created";
}

void WebDAVServer::handle_copy(const HTTPRequest& request, HTTPResponse& response) {
    std::string src_path = decode_url(request.uri);
    
    auto dest_header = request.headers.find("Destination");
    if (dest_header == request.headers.end()) {
        response.status_code = 400;
        response.status_message = "Bad Request";
        return;
    }
    
    std::string dest_path = decode_url(dest_header->second);
    
    if (!file_manager_->copy_resource(src_path, dest_path)) {
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        return;
    }
    
    response.status_code = 201;
    response.status_message = "Created";
}

void WebDAVServer::handle_move(const HTTPRequest& request, HTTPResponse& response) {
    std::string src_path = decode_url(request.uri);
    logger_->info("Handling MOVE request for: " + src_path);
    
    auto dest_header = request.headers.find("Destination");
    if (dest_header == request.headers.end()) {
        logger_->error("Missing Destination header");
        response.status_code = 400;
        response.status_message = "Bad Request";
        return;
    }
    
    // 从 Destination URL 中提取路径
    std::string dest_url = dest_header->second;
    size_t path_start = dest_url.find('/', dest_url.find("://") + 3);
    std::string dest_path;
    if (path_start != std::string::npos) {
        dest_path = decode_url(dest_url.substr(path_start));
    } else {
        logger_->error("Invalid destination URL: " + dest_url);
        response.status_code = 400;
        response.status_message = "Bad Request";
        return;
    }
    
    logger_->info("Moving to path: " + dest_path);
    
    // 检查源文件是否存在
    FileInfo src_info;
    if (!file_manager_->get_resource_info(src_path, src_info)) {
        logger_->error("Source does not exist: " + src_path);
        response.status_code = 404;
        response.status_message = "Not Found";
        return;
    }
    
    // 检查目标父目录是否存在
    std::string dest_parent = dest_path.substr(0, dest_path.find_last_of('/'));
    FileInfo parent_info;
    if (!file_manager_->get_resource_info(dest_parent, parent_info)) {
        logger_->error("Destination parent directory does not exist: " + dest_parent);
        response.status_code = 409;
        response.status_message = "Conflict";
        return;
    }
    
    // 执行移动操作
    if (!file_manager_->move_resource(src_path, dest_path)) {
        logger_->error("Failed to move resource");
        response.status_code = 500;
        response.status_message = "Internal Server Error";
        return;
    }
    
    response.status_code = 201;
    response.status_message = "Created";
    response.headers["Content-Length"] = "0";
    logger_->info("Move operation completed successfully");
}

void WebDAVServer::handle_propfind(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    FileInfo info;
    
    response.headers["Cache-Control"] = "no-cache";
    response.headers["Connection"] = "Keep-Alive";
    response.headers["Keep-Alive"] = "timeout=5, max=100";
    
    if (!file_manager_->get_resource_info(path, info)) {
        response.status_code = 404;
        response.status_message = "Not Found";
        return;
    }
    
    // 检查 Depth 头，默认为 infinity
    auto depth_header = request.headers.find("Depth");
    int depth = (depth_header == request.headers.end() || depth_header->second == "infinity") 
                ? -1 : std::stoi(depth_header->second);
    
    std::string xml_response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                              "<D:multistatus xmlns:D=\"DAV:\">\n";
    
    xml_response += build_xml_response(request.uri, info);
    
    // 只有在 depth > 0 且是目录时才列出内容
    if (info.is_directory && depth != 0) {
        std::vector<FileInfo> items;
        if (file_manager_->list_directory(path, items)) {
            for (const auto& item : items) {
                xml_response += build_xml_response(request.uri + "/" + item.name, item);
            }
        }
    }
    
    xml_response += "</D:multistatus>";
    
    response.status_code = 207;
    response.status_message = "Multi-Status";
    response.headers["Content-Type"] = "application/xml; charset=utf-8";
    response.headers["Content-Length"] = std::to_string(xml_response.length());
    response.body = std::vector<char>(xml_response.begin(), xml_response.end());
}

void WebDAVServer::handle_proppatch(const HTTPRequest& request, HTTPResponse& response) {
    std::string path = decode_url(request.uri);
    logger_->info("Handling PROPPATCH request for: " + path);
    
    // 直接返回成功，不实际修改属性
    response.status_code = 207;  // Multi-Status
    response.status_message = "Multi-Status";
    response.headers["Content-Type"] = "application/xml; charset=utf-8";
    
    // 构建响应 XML
    std::string xml_response = 
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<D:multistatus xmlns:D=\"DAV:\">\n"
        "  <D:response>\n"
        "    <D:href>" + request.uri + "</D:href>\n"
        "    <D:propstat>\n"
        "      <D:prop>\n"
        "        <Win32LastModifiedTime/>\n"
        "        <Win32FileAttributes/>\n"
        "        <Win32CreationTime/>\n"
        "        <Win32LastAccessTime/>\n"
        "      </D:prop>\n"
        "      <D:status>HTTP/1.1 200 OK</D:status>\n"
        "    </D:propstat>\n"
        "  </D:response>\n"
        "</D:multistatus>";
    
    response.body = std::vector<char>(xml_response.begin(), xml_response.end());
    response.headers["Content-Length"] = std::to_string(response.body.size());
}

void WebDAVServer::handle_head(const HTTPRequest& request, HTTPResponse& response) {
    // HEAD 方法与 GET 相同，但不返回响应体
    handle_get(request, response);
    response.body.clear();
}

} // namespace webdav 