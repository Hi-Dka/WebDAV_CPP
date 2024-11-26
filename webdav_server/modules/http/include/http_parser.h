#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "http_types.h"
#include "logger.h"

namespace webdav {

class HTTPParser {
public:
    HTTPParser(Logger& logger) : logger_(logger) {}
    ~HTTPParser();

    bool parse_request(const std::vector<char>& raw_data, HTTPRequest& request);
    std::vector<char> build_response(const HTTPResponse& response);

private:
    HTTPMethod parse_method(const std::string& method_str);
    bool parse_request_line(const std::string& line, HTTPRequest& request);
    bool parse_headers(std::istringstream& stream, std::map<std::string, std::string>& headers);

    Logger& logger_;
};

} // namespace webdav

#endif // HTTP_PARSER_H 