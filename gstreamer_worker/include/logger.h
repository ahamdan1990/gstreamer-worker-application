#ifndef GSTREAMER_WORKER_LOGGER_H
#define GSTREAMER_WORKER_LOGGER_H

#include <string>
#include <sstream>
#include <mutex>
#include <iostream>

namespace gstreamer_worker {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Thread-safe logger
 */
class Logger {
public:
    /**
     * @brief Get logger instance (singleton)
     */
    static Logger& instance();

    /**
     * @brief Set minimum log level
     */
    void set_log_level(LogLevel level);

    /**
     * @brief Log a message
     */
    void log(LogLevel level, const std::string& tag, const std::string& message);

    /**
     * @brief Log debug message
     */
    void debug(const std::string& tag, const std::string& message);

    /**
     * @brief Log info message
     */
    void info(const std::string& tag, const std::string& message);

    /**
     * @brief Log warning message
     */
    void warning(const std::string& tag, const std::string& message);

    /**
     * @brief Log error message
     */
    void error(const std::string& tag, const std::string& message);

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;

    std::string level_to_string(LogLevel level) const;
    std::string get_timestamp() const;
};

// Convenience macros
#define LOG_DEBUG(tag, msg) \
    Logger::instance().debug(tag, msg)

#define LOG_INFO(tag, msg) \
    Logger::instance().info(tag, msg)

#define LOG_WARNING(tag, msg) \
    Logger::instance().warning(tag, msg)

#define LOG_ERROR(tag, msg) \
    Logger::instance().error(tag, msg)

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_LOGGER_H
