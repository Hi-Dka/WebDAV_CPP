#include "http_parser.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace webdav {

HTTPParser::~HTTPParser() {}

HTTPMethod HTTPParser::parse_method(const std::string& method_str) {
    static const std::map<std::string, HTTPMethod> method_map = {
        {"OPTIONS", HTTPMethod::OPTIONS},
        {"GET", HTTPMethod::GET},
        {"PUT", HTTPMethod::PUT},
        {"POST", HTTPMethod::POST},
        {"DELETE", HTTPMethod::DELETE},
        {"PROPFIND", HTTPMethod::PROPFIND},
        {"PROPPATCH", HTTPMethod::PROPPATCH},
        {"MKCOL", HTTPMethod::MKCOL},
        {"COPY", HTTPMethod::COPY},
        {"MOVE", HTTPMethod::MOVE},
        {"LOCK", HTTPMethod::LOCK},
        {"UNLOCK", HTTPMethod::UNLOCK},
        {"HEAD", HTTPMethod::HEAD}
    };
    
    auto it = method_map.find(method_str);
    if (it != method_map.end()) {
        return it->second;
    }
    
    logger_.error("Unknown HTTP method: [" + method_str + "]");
    return HTTPMethod::UNKNOWN;
}

bool HTTPParser::parse_request_line(const std::string& line, HTTPRequest& request) {
    std::istringstream iss(line);
    std::string method_str;
    
    if (!(iss >> method_str >> request.uri >> request.version)) {
        logger_.error("Failed to parse request line: [" + line + "]");
        return false;
    }
    
    // 移除可能的空白字符
    method_str.erase(0, method_str.find_first_not_of(" \t\r\n"));
    method_str.erase(method_str.find_last_not_of(" \t\r\n") + 1);
    
    request.method = parse_method(method_str);
    if (request.method == HTTPMethod::UNKNOWN) {
        logger_.error("Unknown method string: [" + method_str + "]");
    }
    
    return true;
}

bool HTTPParser::parse_headers(std::istringstream& stream, std::map<std::string, std::string>& headers) {
    std::string line;
    while (std::getline(stream, line) && line != "\r" && line != "") {
        // 移除行尾的 \r\n
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // 跳过空行
        if (line.empty()) {
            continue;
        }
        
        // 查找冒号位置
        size_t pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        
        // 提取键和值
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 移除前后空白
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);
        
        // 验证头部字段名是否合法（只允许可打印ASCII字符）
        bool valid_key = true;
        for (char c : key) {
            if (c < 32 || c > 126) {
                valid_key = false;
                break;
            }
        }
        
        // 验证头部字段值是否合法（只允许可打印ASCII字符和空格）
        bool valid_value = true;
        for (char c : value) {
            if ((c < 32 && c != ' ' && c != '\t') || c > 126) {
                valid_value = false;
                break;
            }
        }
        
        // 只添加合法的头部
        if (valid_key && valid_value) {
            headers[key] = value;
        }
    }
    return true;
}

bool HTTPParser::parse_request(const std::vector<char>& raw_data, HTTPRequest& request) {
    // 检查数据大小
    if (raw_data.empty()) {
        logger_.error("Empty request data");
        return false;
    }
    
    logger_.debug("Parsing request with " + std::to_string(raw_data.size()) + " bytes");
    
    // 查找请求头结束位置
    const char* data = raw_data.data();
    size_t size = raw_data.size();
    const char* headers_end = nullptr;
    
    // 查找 "\r\n\r\n"
    for (size_t i = 0; i + 3 < size; i++) {
        if (data[i] == '\r' && data[i+1] == '\n' && 
            data[i+2] == '\r' && data[i+3] == '\n') {
            headers_end = data + i + 4;
            break;
        }
    }
    
    if (!headers_end) {
        logger_.error("No header end marker found");
        return false;
    }
    
    // 打印原始请求头
    std::string raw_headers(data, headers_end);
    logger_.debug("Raw request:\n" + raw_headers);
    
    // 解析请求头
    std::string headers_str(data, headers_end - data);
    std::istringstream stream(headers_str);
    
    // 解析请求行
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        logger_.error("Failed to read request line");
        return false;
    }
    
    // 移除行尾的 \r
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }
    
    logger_.debug("Request line: [" + request_line + "]");
    
    // 解析请求行
    if (!parse_request_line(request_line, request)) {
        logger_.error("Failed to parse request line: " + request_line);
        return false;
    }
    
    // 解析头部
    if (!parse_headers(stream, request.headers)) {
        logger_.error("Failed to parse headers");
        return false;
    }
    
    // 打印解析后的头部
    logger_.debug("Parsed headers:");
    for (const auto& header : request.headers) {
        logger_.debug("  " + header.first + ": " + header.second);
    }
    
    // 处理请求体
    auto content_length_it = request.headers.find("Content-Length");
    if (content_length_it != request.headers.end()) {
        size_t content_length = std::stoul(content_length_it->second);
        size_t headers_size = headers_end - data;
        
        logger_.debug("Content-Length: " + std::to_string(content_length));
        logger_.debug("Headers size: " + std::to_string(headers_size));
        logger_.debug("Total data size: " + std::to_string(size));
        
        // 如果有 Content-Length 但是是 0，不需要处理请求体
        if (content_length == 0) {
            return true;
        }
        
        // 检查是否有足够的数据
        if (headers_size + content_length <= size) {
            request.body.assign(headers_end, headers_end + content_length);
            logger_.debug("Body size: " + std::to_string(request.body.size()));
            return true;
        } else {
            logger_.error("Incomplete body: expected " + std::to_string(content_length) + 
                         " bytes but only got " + std::to_string(size - headers_size));
            return false;
        }
    }
    
    return true;
}

std::vector<char> HTTPParser::build_response(const HTTPResponse& response) {
    std::ostringstream oss;
    
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_message << "\r\n";
    
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    oss << "\r\n";
    
    std::string headers = oss.str();
    std::vector<char> result(headers.begin(), headers.end());
    result.insert(result.end(), response.body.begin(), response.body.end());
    
    return result;
}

} // namespace webdav 