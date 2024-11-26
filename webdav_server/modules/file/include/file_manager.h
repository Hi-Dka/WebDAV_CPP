#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <ctime>
#include "file_types.h"

namespace webdav {

class Logger;

class FileManager {
public:
    FileManager(const std::string& root_path, Logger& logger);
    ~FileManager();

    bool create_directory(const std::string& path);
    bool delete_resource(const std::string& path);
    bool copy_resource(const std::string& src_path, const std::string& dest_path);
    bool move_resource(const std::string& src_path, const std::string& dest_path);
    bool write_file(const std::string& path, const std::vector<char>& data);
    bool read_file(const std::string& path, std::vector<char>& data);
    bool get_resource_info(const std::string& path, FileInfo& info);
    bool list_directory(const std::string& path, std::vector<FileInfo>& items);
    bool set_properties(const std::string& path, const std::map<std::string, std::string>& properties);
    bool get_properties(const std::string& path, std::map<std::string, std::string>& properties);

    bool write_file_direct(const std::string& path, const std::vector<char>& data, size_t offset = 0);
    bool write_file_stream(const std::string& path, int* fd_out = nullptr);
    bool finish_write(const std::string& path, int fd);

private:
    std::string normalize_path(const std::string& path);
    std::string get_absolute_path(const std::string& relative_path);
    bool check_path_security(const std::string& path);
    std::string generate_etag(const std::string& path);

    std::string root_path_;
    Logger& logger_;
    std::mutex file_mutex_;
    std::map<std::string, std::mutex> path_mutexes_;
    std::mutex path_mutexes_mutex_;

    struct CacheEntry {
        FileInfo info;
        time_t cache_time;
    };
    
    std::map<std::string, CacheEntry> cache_;
    std::mutex cache_mutex_;
    static const int CACHE_TTL = 5; // 缓存有效期（秒）
};

} // namespace webdav

#endif // FILE_MANAGER_H 