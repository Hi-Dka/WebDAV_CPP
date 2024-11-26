#ifndef FILE_TYPES_H
#define FILE_TYPES_H

#include <string>
#include <map>

namespace webdav {

struct FileInfo {
    std::string name;
    std::string path;
    size_t size;
    time_t created_time;
    time_t modified_time;
    time_t accessed_time;
    bool is_directory;
    std::string etag;
    std::map<std::string, std::string> properties;
};

} // namespace webdav

#endif // FILE_TYPES_H 