#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#include <string>
#include <map>

namespace webdav {

class MimeTypes {
public:
    static std::string get_mime_type(const std::string& path);
    
private:
    static std::map<std::string, std::string> mime_types;
    static void init_mime_types();
    static bool initialized;
};

} // namespace webdav

#endif // MIME_TYPES_H 