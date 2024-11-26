#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <vector>

namespace webdav {

class Base64 {
public:
    static std::string encode(const std::vector<char>& data);
    static std::vector<char> decode(const std::string& input);
    
private:
    static const std::string base64_chars;
    static inline bool is_base64(unsigned char c);
};

} // namespace webdav

#endif // BASE64_H 