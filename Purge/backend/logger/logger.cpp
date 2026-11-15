#define _CRT_SECURE_NO_WARNINGS
#include "logger.h"

LogLevel Logger::currentLevel = LogLevel::INFO;
bool Logger::showTimestamp = true;

void Logger::setLevel(LogLevel level) {
    currentLevel = level;
}

void Logger::setTimestamp(bool enabled) {
    showTimestamp = enabled;
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:   return "DEBUG";
    case LogLevel::INFO:    return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERRORLOG:   return "ERROR";
    default:                return "UNKNOWN";
    }
}

std::string Logger::getColorCode(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG:   return "\033[36m";  
    case LogLevel::INFO:    return "\033[32m";  
    case LogLevel::WARNING: return "\033[33m";  
    case LogLevel::ERRORLOG:   return "\033[31m";
    default:                return "\033[0m";  
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel) {
        return;
    }

    std::ostringstream oss;

    oss << getColorCode(level);

    if (showTimestamp) {
        oss << "[" << getCurrentTimestamp() << "] ";
    }

    oss << "[" << levelToString(level) << "] ";

    oss << message;

    oss << "\033[0m";

    if (level >= LogLevel::ERRORLOG) {
        std::cerr << oss.str() << std::endl;
    }
    else {
        std::cout << oss.str() << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERRORLOG, message);
}