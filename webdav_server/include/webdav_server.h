#ifndef WEBDAV_SERVER_H
#define WEBDAV_SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <map>
#include "http_types.h"
#include "file_types.h"
#include "logger.h"
#include "auth_manager.h"
#include "http_parser.h"
#include "file_manager.h"
#include "xml_parser.h"

namespace webdav {

class WebDAVServer {
public:
    WebDAVServer(const std::string& host, int port, const std::string& root_path);
    ~WebDAVServer();

    bool start();
    void stop();

private:
    void accept_connections();
    void handle_client(int client_socket);
    void handle_request(const HTTPRequest& request, HTTPResponse& response);
    
    // WebDAV 方法处理函数
    void handle_options(const HTTPRequest& request, HTTPResponse& response);
    void handle_get(const HTTPRequest& request, HTTPResponse& response);
    void handle_put(const HTTPRequest& request, HTTPResponse& response);
    void handle_delete(const HTTPRequest& request, HTTPResponse& response);
    void handle_mkcol(const HTTPRequest& request, HTTPResponse& response);
    void handle_copy(const HTTPRequest& request, HTTPResponse& response);
    void handle_move(const HTTPRequest& request, HTTPResponse& response);
    void handle_propfind(const HTTPRequest& request, HTTPResponse& response);
    void handle_proppatch(const HTTPRequest& request, HTTPResponse& response);
    void handle_head(const HTTPRequest& request, HTTPResponse& response);
    void handle_lock_request(int client_socket);
    
    // 辅助函数
    std::string build_xml_response(const std::string& uri, const FileInfo& info);
    std::string decode_url(const std::string& url);
    bool authenticate(const HTTPRequest& request);
    std::string format_http_date(time_t t);
    std::string format_iso_date(time_t t);

    void send_error_response(int client_socket, int status_code, const std::string& status_message);

    std::string host_;
    int port_;
    std::string root_path_;
    int server_socket_;
    std::atomic<bool> running_;
    
    std::vector<std::thread> worker_threads_;
    std::mutex threads_mutex_;
    
    std::unique_ptr<Logger> logger_;
    std::unique_ptr<AuthManager> auth_manager_;
    std::unique_ptr<HTTPParser> http_parser_;
    std::unique_ptr<FileManager> file_manager_;
    std::unique_ptr<XMLParser> xml_parser_;
};

} // namespace webdav

#endif // WEBDAV_SERVER_H 