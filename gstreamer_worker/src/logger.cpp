#include "logger.h"
#include <iomanip>
#include <chrono>
#include <ctime>

namespace gstreamer_worker {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::log(LogLevel level, const std::string& tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (level < min_level_) {
        return;
    }

    std::cout << get_timestamp() << " "
              << "[" << level_to_string(level) << "] "
              << "[" << tag << "] "
              << message << std::endl;
}

void Logger::debug(const std::string& tag, const std::string& message) {
    log(LogLevel::DEBUG, tag, message);
}

void Logger::info(const std::string& tag, const std::string& message) {
    log(LogLevel::INFO, tag, message);
}

void Logger::warning(const std::string& tag, const std::string& message) {
    log(LogLevel::WARNING, tag, message);
}

void Logger::error(const std::string& tag, const std::string& message) {
    log(LogLevel::ERROR, tag, message);
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO ";
        case LogLevel::WARNING: return "WARN ";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

} // namespace gstreamer_worker
