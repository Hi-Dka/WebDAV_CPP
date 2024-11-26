#include "auth_manager.h"
#include <sstream>
#include <iomanip>

namespace webdav {

AuthManager::AuthManager() {
    add_user("admin", "admin123");
}

AuthManager::~AuthManager() {}

std::string AuthManager::hash_password(const std::string& password) {
    unsigned long hash = 5381;
    for (char c : password) {
        hash = ((hash << 5) + hash) + c;
    }
    
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

bool AuthManager::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    return it->second == hash_password(password);
}

bool AuthManager::add_user(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    
    if (users_.find(username) != users_.end()) {
        return false;
    }
    
    users_[username] = hash_password(password);
    return true;
}

bool AuthManager::remove_user(const std::string& username) {
    std::lock_guard<std::mutex> lock(users_mutex_);
    return users_.erase(username) > 0;
}

} // namespace webdav 