#include "webdav_server.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>

using namespace webdav;

WebDAVServer* server = nullptr;

void signal_handler(int signum) {
    if (server) {
        std::cout << "Stopping server..." << std::endl;
        server->stop();
    }
    exit(signum);
}

void print_usage() {
    std::cout << "Usage: webdav_server [options]\n"
              << "Options:\n"
              << "  --host HOST     Server host address (default: 0.0.0.0)\n"
              << "  --port PORT     Server port (default: 8080)\n"
              << "  --root PATH     Root directory path (default: ./webdav_root)\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // 创建日志目录
    if (mkdir("logs", 0755) != 0 && errno != EEXIST) {
        std::cerr << "Failed to create logs directory: " << strerror(errno) << std::endl;
    }
    
    // 创建根目录
    if (mkdir("webdav_root", 0755) != 0 && errno != EEXIST) {
        std::cerr << "Failed to create root directory: " << strerror(errno) << std::endl;
    }
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // 默认配置
    std::string host = "0.0.0.0";
    int port = 8080;
    std::string root_path = "./webdav_root";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--root" && i + 1 < argc) {
            root_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage();
            return 1;
        }
    }
    
    try {
        // 创建并启动服务器
        server = new WebDAVServer(host, port, root_path);
        
        if (!server->start()) {
            std::cerr << "Failed to start server" << std::endl;
            delete server;
            return 1;
        }
        
        std::cout << "WebDAV server started on " << host << ":" << port << std::endl;
        std::cout << "Root directory: " << root_path << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        // 主循环
        while (true) {
            sleep(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        if (server) {
            delete server;
        }
        return 1;
    }
    
    return 0;
} 