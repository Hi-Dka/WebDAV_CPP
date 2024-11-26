#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

namespace webdav {

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };

    Logger(const std::string& filename, Level min_level = Level::DEBUG)
        : filename_(filename), min_level_(min_level) {
        log_file_.open(filename_, std::ios::app);
        if (!log_file_.is_open()) {
            throw std::runtime_error("Failed to open log file: " + filename_);
        }
    }

    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    void debug(const std::string& message) {
        if (min_level_ <= Level::DEBUG) {
            log(Level::DEBUG, message);
        }
    }

    void info(const std::string& message) {
        if (min_level_ <= Level::INFO) {
            log(Level::INFO, message);
        }
    }

    void warning(const std::string& message) {
        if (min_level_ <= Level::WARNING) {
            log(Level::WARNING, message);
        }
    }

    void error(const std::string& message) {
        if (min_level_ <= Level::ERROR) {
            log(Level::ERROR, message);
        }
    }

    void set_level(Level level) {
        min_level_ = level;
    }

private:
    void log(Level level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string level_str;
        switch (level) {
            case Level::DEBUG:   level_str = "DEBUG"; break;
            case Level::INFO:    level_str = "INFO"; break;
            case Level::WARNING: level_str = "WARNING"; break;
            case Level::ERROR:   level_str = "ERROR"; break;
        }
        
        std::string log_entry = get_current_time() + " [" + level_str + "] " + message;
        
        log_file_ << log_entry << std::endl;
        log_file_.flush();
        std::cout << log_entry << std::endl;
    }

    std::string get_current_time();

    std::ofstream log_file_;
    std::mutex mutex_;
    std::string filename_;
    Level min_level_;
};

} // namespace webdav

#endif // LOGGER_H 