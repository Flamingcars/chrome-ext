#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERRORLOG = 3
};

class Logger {
private:
    static LogLevel currentLevel;
    static bool showTimestamp;
    static std::string getCurrentTimestamp();
    static std::string levelToString(LogLevel level);
    static std::string getColorCode(LogLevel level);
    static void log(LogLevel level, const std::string& message);

public:
    static void setLevel(LogLevel level);

    static void setTimestamp(bool enabled);

    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

    template<typename... Args>
    static void debug(const std::string& format, Args... args) {
        if (currentLevel <= LogLevel::DEBUG) {
            std::ostringstream oss;
            formatMessage(oss, format, args...);
            debug(oss.str());
        }
    }

    template<typename... Args>
    static void info(const std::string& format, Args... args) {
        if (currentLevel <= LogLevel::INFO) {
            std::ostringstream oss;
            formatMessage(oss, format, args...);
            info(oss.str());
        }
    }

    template<typename... Args>
    static void warning(const std::string& format, Args... args) {
        if (currentLevel <= LogLevel::WARNING) {
            std::ostringstream oss;
            formatMessage(oss, format, args...);
            warning(oss.str());
        }
    }

    template<typename... Args>
    static void error(const std::string& format, Args... args) {
        if (currentLevel <= LogLevel::ERRORLOG) {
            std::ostringstream oss;
            formatMessage(oss, format, args...);
            error(oss.str());
        }
    }

private:
    template<typename T>
    static void formatMessage(std::ostringstream& oss, const std::string& format, T&& value) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << value << format.substr(pos + 2);
        }
        else {
            oss << format;
        }
    }

    template<typename T, typename... Args>
    static void formatMessage(std::ostringstream& oss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            std::string newFormat = format.substr(0, pos) + std::to_string(value) + format.substr(pos + 2);
            formatMessage(oss, newFormat, args...);
        }
        else {
            oss << format;
        }
    }
};

#define LOG_DEBUG(msg) Logger::debug(msg)
#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARNING(msg) Logger::warning(msg)
#define LOG_ERROR(msg) Logger::error(msg)

#define LOG_DEBUG_F(format, ...) Logger::debug(format, __VA_ARGS__)
#define LOG_INFO_F(format, ...) Logger::info(format, __VA_ARGS__)
#define LOG_WARNING_F(format, ...) Logger::warning(format, __VA_ARGS__)
#define LOG_ERROR_F(format, ...) Logger::error(format, __VA_ARGS__)

#endif