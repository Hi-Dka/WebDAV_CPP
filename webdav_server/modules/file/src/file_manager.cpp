#include "file_manager.h"
#include "logger.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace webdav {

FileManager::FileManager(const std::string& root_path, Logger& logger)
    : root_path_(root_path), logger_(logger) {
    if (mkdir(root_path.c_str(), 0755) != 0 && errno != EEXIST) {
        logger_.error("Failed to create root directory: " + root_path);
    }
}

FileManager::~FileManager() {}

std::string FileManager::normalize_path(const std::string& path) {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    
    auto it = std::unique(result.begin(), result.end(),
                         [](char a, char b) { return a == '/' && b == '/'; });
    result.erase(it, result.end());
    
    if (result.length() > 1 && result.back() == '/') {
        result.pop_back();
    }
    
    return result;
}

std::string FileManager::get_absolute_path(const std::string& relative_path) {
    return normalize_path(root_path_ + "/" + relative_path);
}

bool FileManager::check_path_security(const std::string& path) {
    std::string abs_path = get_absolute_path(path);
    std::string norm_root = normalize_path(root_path_);
    return abs_path.substr(0, norm_root.length()) == norm_root;
}

std::string FileManager::generate_etag(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return "";
    }
    
    std::stringstream ss;
    ss << std::hex << st.st_mtime << "-" << st.st_size;
    return "\"" + ss.str() + "\"";
}

bool FileManager::create_directory(const std::string& path) {
    if (!check_path_security(path)) {
        logger_.error("Security check failed for path: " + path);
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    return mkdir(abs_path.c_str(), 0755) == 0;
}

bool FileManager::delete_resource(const std::string& path) {
    if (!check_path_security(path)) {
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    struct stat st;
    
    if (stat(abs_path.c_str(), &st) != 0) {
        return false;
    }
    
    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(abs_path.c_str());
        if (!dir) return false;
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string sub_path = path + "/" + entry->d_name;
            delete_resource(sub_path);
        }
        
        closedir(dir);
        return rmdir(abs_path.c_str()) == 0;
    } else {
        return unlink(abs_path.c_str()) == 0;
    }
}

bool FileManager::copy_resource(const std::string& src_path, const std::string& dest_path) {
    if (!check_path_security(src_path) || !check_path_security(dest_path)) {
        return false;
    }
    
    std::string abs_src = get_absolute_path(src_path);
    std::string abs_dest = get_absolute_path(dest_path);
    
    struct stat st;
    if (stat(abs_src.c_str(), &st) != 0) {
        return false;
    }
    
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(abs_dest.c_str(), st.st_mode) != 0) {
            return false;
        }
        
        DIR* dir = opendir(abs_src.c_str());
        if (!dir) return false;
        
        bool success = true;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string sub_src = src_path + "/" + entry->d_name;
            std::string sub_dest = dest_path + "/" + entry->d_name;
            
            if (!copy_resource(sub_src, sub_dest)) {
                success = false;
                break;
            }
        }
        
        closedir(dir);
        return success;
    } else {
        std::ifstream src(abs_src, std::ios::binary);
        std::ofstream dest(abs_dest, std::ios::binary);
        
        if (!src || !dest) {
            return false;
        }
        
        dest << src.rdbuf();
        return true;
    }
}

bool FileManager::move_resource(const std::string& src_path, const std::string& dest_path) {
    if (!check_path_security(src_path) || !check_path_security(dest_path)) {
        logger_.error("Security check failed for move operation: src=" + src_path + ", dest=" + dest_path);
        return false;
    }
    
    std::string abs_src = get_absolute_path(src_path);
    std::string abs_dest = get_absolute_path(dest_path);
    
    logger_.info("Moving resource from " + abs_src + " to " + abs_dest);
    
    // 检查源文件是否存在
    struct stat src_stat;
    if (stat(abs_src.c_str(), &src_stat) != 0) {
        logger_.error("Source does not exist: " + abs_src + " (errno: " + std::to_string(errno) + ")");
        return false;
    }
    
    // 检查目标父目录是否存在
    std::string dest_parent = abs_dest.substr(0, abs_dest.find_last_of('/'));
    struct stat parent_stat;
    if (stat(dest_parent.c_str(), &parent_stat) != 0) {
        logger_.error("Destination parent directory does not exist: " + dest_parent);
        return false;
    }
    
    // 检查目标是否已存在
    struct stat dest_stat;
    bool dest_exists = (stat(abs_dest.c_str(), &dest_stat) == 0);
    
    if (dest_exists) {
        // 如果源和目标都是目录，或者都是文件，才允许覆盖
        bool src_is_dir = S_ISDIR(src_stat.st_mode);
        bool dest_is_dir = S_ISDIR(dest_stat.st_mode);
        
        if (src_is_dir != dest_is_dir) {
            logger_.error("Cannot overwrite: source and destination types do not match");
            return false;
        }
        
        // 如果是目录，确保目标目录为空
        if (dest_is_dir) {
            DIR* dir = opendir(abs_dest.c_str());
            if (!dir) {
                logger_.error("Failed to open destination directory: " + abs_dest);
                return false;
            }
            
            bool is_empty = true;
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    is_empty = false;
                    break;
                }
            }
            closedir(dir);
            
            if (!is_empty) {
                logger_.error("Destination directory is not empty: " + abs_dest);
                return false;
            }
        }
    }
    
    // 尝试直接重命名
    if (rename(abs_src.c_str(), abs_dest.c_str()) == 0) {
        // 清除缓存
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            cache_.erase(abs_src);
            cache_.erase(abs_dest);
            logger_.debug("Cleared cache entries for both source and destination");
        }
        
        logger_.info("Successfully moved resource");
        return true;
    }
    
    // 如果重命名失败，尝试复制然后删除
    if (copy_resource(src_path, dest_path)) {
        if (delete_resource(src_path)) {
            logger_.info("Successfully moved resource (copy and delete)");
            return true;
        } else {
            // 如果删除源文件失败，也要删除刚复制的目标文件
            delete_resource(dest_path);
            logger_.error("Failed to delete source after copy");
            return false;
        }
    }
    
    logger_.error("Failed to move resource: " + std::string(strerror(errno)));
    return false;
}

bool FileManager::write_file(const std::string& path, const std::vector<char>& data) {
    if (!check_path_security(path)) {
        logger_.error("Security check failed for path: " + path);
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    logger_.info("Writing file: " + abs_path + " (size: " + std::to_string(data.size()) + " bytes)");
    
    // 创建父目录（如果不存在）
    std::string parent_path = abs_path.substr(0, abs_path.find_last_of('/'));
    if (mkdir(parent_path.c_str(), 0755) != 0 && errno != EEXIST) {
        logger_.error("Failed to create parent directory: " + parent_path);
        return false;
    }
    
    // 打开文件
    int fd = open(abs_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        logger_.error("Failed to open file: " + std::string(strerror(errno)));
        return false;
    }
    
    // 写入数据
    const char* buf = data.data();
    size_t remaining = data.size();
    while (remaining > 0) {
        ssize_t written = write(fd, buf, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            logger_.error("Failed to write data: " + std::string(strerror(errno)));
            close(fd);
            return false;
        }
        buf += written;
        remaining -= written;
    }
    
    // 同步文件到磁盘
    if (fsync(fd) < 0) {
        logger_.error("Failed to sync file: " + std::string(strerror(errno)));
    }
    
    close(fd);
    
    // 清除缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.erase(abs_path);
    }
    
    logger_.info("Successfully wrote file: " + abs_path);
    return true;
}

bool FileManager::read_file(const std::string& path, std::vector<char>& data) {
    if (!check_path_security(path)) {
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    std::ifstream file(abs_path, std::ios::binary | std::ios::ate);
    
    if (!file) {
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    data.resize(size);
    file.read(data.data(), size);
    
    return true;
}

bool FileManager::get_resource_info(const std::string& path, FileInfo& info) {
    if (!check_path_security(path)) {
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    
    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(abs_path);
        if (it != cache_.end()) {
            time_t now = time(nullptr);
            if (now - it->second.cache_time < CACHE_TTL) {
                info = it->second.info;
                return true;
            }
            cache_.erase(it);
        }
    }
    
    struct stat st;
    if (stat(abs_path.c_str(), &st) != 0) {
        return false;
    }
    
    info.name = path.substr(path.find_last_of('/') + 1);
    info.path = path;
    info.size = st.st_size;
    info.created_time = st.st_ctime;
    info.modified_time = st.st_mtime;
    info.accessed_time = st.st_atime;
    info.is_directory = S_ISDIR(st.st_mode);
    info.etag = generate_etag(abs_path);
    
    // 更新缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_[abs_path] = {info, time(nullptr)};
    }
    
    return true;
}

bool FileManager::list_directory(const std::string& path, std::vector<FileInfo>& items) {
    if (!check_path_security(path)) {
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    DIR* dir = opendir(abs_path.c_str());
    
    if (!dir) {
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        FileInfo info;
        std::string sub_path = path + "/" + entry->d_name;
        
        if (get_resource_info(sub_path, info)) {
            items.push_back(info);
        }
    }
    
    closedir(dir);
    return true;
}

bool FileManager::set_properties(const std::string& path, 
                               const std::map<std::string, std::string>& properties) {
    if (!check_path_security(path)) {
        return false;
    }
    
    FileInfo info;
    if (!get_resource_info(path, info)) {
        return false;
    }
    
    info.properties = properties;
    return true;
}

bool FileManager::get_properties(const std::string& path, 
                               std::map<std::string, std::string>& properties) {
    if (!check_path_security(path)) {
        return false;
    }
    
    FileInfo info;
    if (!get_resource_info(path, info)) {
        return false;
    }
    
    properties = info.properties;
    return true;
}

bool FileManager::write_file_stream(const std::string& path, int* fd_out) {
    if (!check_path_security(path)) {
        logger_.error("Security check failed for path: " + path);
        return false;
    }
    
    std::string abs_path = get_absolute_path(path);
    logger_.info("Opening file for writing: " + abs_path);
    
    // 创建父目录（如果不存在）
    std::string parent_path = abs_path.substr(0, abs_path.find_last_of('/'));
    if (mkdir(parent_path.c_str(), 0755) != 0 && errno != EEXIST) {
        logger_.error("Failed to create parent directory: " + parent_path);
        return false;
    }
    
    // 打开文件
    int fd = open(abs_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        logger_.error("Failed to open file: " + std::string(strerror(errno)));
        return false;
    }
    
    if (fd_out) {
        *fd_out = fd;
    }
    
    return true;
}

bool FileManager::finish_write(const std::string& path, int fd) {
    if (fd < 0) return false;
    
    // 同步文件到磁盘
    if (fsync(fd) < 0) {
        logger_.error("Failed to sync file: " + std::string(strerror(errno)));
    }
    
    close(fd);
    
    // 清除缓存
    std::string abs_path = get_absolute_path(path);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.erase(abs_path);
    }
    
    logger_.info("Successfully finished writing file: " + abs_path);
    return true;
}

} // namespace webdav 