#ifndef HTTP_TYPES_H
#define HTTP_TYPES_H

#include <string>
#include <map>
#include <vector>

namespace webdav {

enum class HTTPMethod {
    GET,
    PUT,
    POST,
    DELETE,
    PROPFIND,
    PROPPATCH,
    MKCOL,
    COPY,
    MOVE,
    LOCK,
    UNLOCK,
    OPTIONS,
    HEAD,
    UNKNOWN
};

struct HTTPRequest {
    HTTPMethod method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::vector<char> body;
};

struct HTTPResponse {
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::vector<char> body;
};

} // namespace webdav

#endif // HTTP_TYPES_H 