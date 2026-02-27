#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLevel(LogLevel level) {
        currentLevel = level;
    }

    template<typename... Args>
    void log(LogLevel level, const char* file, int line, Args... args) {
        if (level < currentLevel) return;

        std::lock_guard<std::mutex> lock(logMutex);
        
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm;
        localtime_r(&now_c, &tm);

        const char* levelStr = "";
        switch (level) {
            case LogLevel::DEBUG:   levelStr = "[DEBUG] "; break;
            case LogLevel::INFO:    levelStr = "[INFO]  "; break;
            case LogLevel::WARNING: levelStr = "[WARN]  "; break;
            case LogLevel::ERROR:   levelStr = "[ERROR] "; break;
        }

        std::ostream& out = (level == LogLevel::ERROR) ? std::cerr : std::cout;

        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << " "
            << levelStr;

        (out << ... << args) << " (" << file << ":" << line << ")\n";
    }

private:
    Logger() : currentLevel(LogLevel::INFO) {}
    std::mutex logMutex;
    LogLevel currentLevel;
};

#define LOG_DEBUG(...)   Logger::getInstance().log(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)    Logger::getInstance().log(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)    Logger::getInstance().log(LogLevel::WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)   Logger::getInstance().log(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif // LOGGER_H
