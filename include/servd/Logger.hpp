#pragma once

#include <mutex>
#include <string_view>
#include <fstream>
#include <string>

namespace servd {

class Logger {
public:
    enum class LogLevel {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void setLevel(LogLevel level);
    static void setLogFile(const std::string& file_path);
    static void log_s(LogLevel level, const char* file, int line, const char* format, ...);
    static void log(LogLevel level, const char* file, int line, const char* format, ...);

private:
    static LogLevel s_level;
    static std::string s_log_file;
    static std::ofstream s_file_stream;
    static std::mutex s_log_mutex;

    static std::string get_formatted_time();
    static void print_sanitized(const char* str);
};

}

#define SERVD_LOGS(level, format, ...) \
    servd::Logger::log_s(level, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define SERVD_LOG(level, format, ...) \
    servd::Logger::log(level, __FILE__, __LINE__, format, ##__VA_ARGS__)
