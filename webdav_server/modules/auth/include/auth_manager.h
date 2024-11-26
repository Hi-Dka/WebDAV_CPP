#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>
#include <map>
#include <mutex>

namespace webdav {

class AuthManager {
public:
    AuthManager();
    ~AuthManager();

    bool authenticate(const std::string& username, const std::string& password);
    bool add_user(const std::string& username, const std::string& password);
    bool remove_user(const std::string& username);

private:
    std::string hash_password(const std::string& password);
    
    std::map<std::string, std::string> users_; // username -> hashed_password
    std::mutex users_mutex_;
};

} // namespace webdav

#endif // AUTH_MANAGER_H 