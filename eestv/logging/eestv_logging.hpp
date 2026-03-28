#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>

namespace eestv
{
namespace logging
{

enum class LogLevel
{
    Error   = 0,
    Warning = 1,
    Info    = 2,
    Debug   = 3,
    Trace   = 4
};

// Global log level variable
extern LogLevel current_log_level;

// Helper function to check if a log level should be printed
inline bool should_log(LogLevel level)
{
    return static_cast<int>(level) <= static_cast<int>(current_log_level);
}

// Constexpr helper to find last occurrence of path separator
constexpr const char* find_last_separator(const char* str, const char* last_found = nullptr)
{
    return *str == '\0' ? (last_found ? last_found : str) : find_last_separator(str + 1, (*str == '/' || *str == '\\') ? str : last_found);
}

// Constexpr helper to extract filename from path
constexpr const char* get_filename(const char* path)
{
    const char* last_sep = find_last_separator(path);
    return (*last_sep == '/' || *last_sep == '\\') ? last_sep + 1 : path;
}

// Constexpr helper to get relative path from source directory
constexpr const char* get_relative_path(const char* file_path, const char* source_dir)
{
    // Find where source_dir ends in file_path
    const char* file = file_path;
    const char* src  = source_dir;

    while (*src != '\0' && *file != '\0')
    {
        if (*src != *file)
        {
            return file_path; // No match, return full path
        }
        ++src;
        ++file;
    }

    return (*src == '\0') ? file : file_path;
}

// Helper function to get log level name
constexpr const char* get_log_level_name(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Trace:
        return "TRACE";
    default:
        return "UNKNOWN";
    }
}

} // namespace logging
} // namespace eestv

// Macro to set the log level
#define EESTV_SET_LOG_LEVEL(level) eestv::logging::current_log_level = eestv::logging::LogLevel::level

// Logging macros that accept stream expressions
#define EESTV_LOG_ERROR(message)                                                                                                  \
    do                                                                                                                            \
    {                                                                                                                             \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Error))                                                          \
        {                                                                                                                         \
            constexpr const char* relative_path = eestv::logging::get_relative_path(__FILE__, EESTV_SOURCE_DIR);                  \
            std::ostringstream oss_msg;                                                                                           \
            oss_msg << message;                                                                                                   \
            std::ostringstream oss_location;                                                                                      \
            oss_location << relative_path << ":" << __LINE__;                                                                     \
            std::ostringstream oss_final;                                                                                         \
            oss_final << std::left << std::setw(7) << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Error) << " "  \
                      << std::setw(55) << oss_location.str() << " " << std::setw(40) << __func__ << " " << oss_msg.str() << "\n"; \
            std::cout << oss_final.str() << std::flush;                                                                           \
        }                                                                                                                         \
    } while (0)

#define EESTV_LOG_WARNING(message)                                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Warning))                                                         \
        {                                                                                                                          \
            constexpr const char* relative_path = eestv::logging::get_relative_path(__FILE__, EESTV_SOURCE_DIR);                   \
            std::ostringstream oss_msg;                                                                                            \
            oss_msg << message;                                                                                                    \
            std::ostringstream oss_location;                                                                                       \
            oss_location << relative_path << ":" << __LINE__;                                                                      \
            std::ostringstream oss_final;                                                                                          \
            oss_final << std::left << std::setw(7) << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Warning) << " " \
                      << std::setw(55) << oss_location.str() << " " << std::setw(40) << __func__ << " " << oss_msg.str() << "\n";  \
            std::cout << oss_final.str() << std::flush;                                                                            \
        }                                                                                                                          \
    } while (0)

#define EESTV_LOG_INFO(message)                                                                                                   \
    do                                                                                                                            \
    {                                                                                                                             \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Info))                                                           \
        {                                                                                                                         \
            constexpr const char* relative_path = eestv::logging::get_relative_path(__FILE__, EESTV_SOURCE_DIR);                  \
            std::ostringstream oss_msg;                                                                                           \
            oss_msg << message;                                                                                                   \
            std::ostringstream oss_location;                                                                                      \
            oss_location << relative_path << ":" << __LINE__;                                                                     \
            std::ostringstream oss_final;                                                                                         \
            oss_final << std::left << std::setw(7) << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Info) << " "   \
                      << std::setw(55) << oss_location.str() << " " << std::setw(40) << __func__ << " " << oss_msg.str() << "\n"; \
            std::cout << oss_final.str() << std::flush;                                                                           \
        }                                                                                                                         \
    } while (0)

#define EESTV_LOG_DEBUG(message)                                                                                                  \
    do                                                                                                                            \
    {                                                                                                                             \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Debug))                                                          \
        {                                                                                                                         \
            constexpr const char* relative_path = eestv::logging::get_relative_path(__FILE__, EESTV_SOURCE_DIR);                  \
            std::ostringstream oss_msg;                                                                                           \
            oss_msg << message;                                                                                                   \
            std::ostringstream oss_location;                                                                                      \
            oss_location << relative_path << ":" << __LINE__;                                                                     \
            std::ostringstream oss_final;                                                                                         \
            oss_final << std::left << std::setw(7) << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Debug) << " "  \
                      << std::setw(55) << oss_location.str() << " " << std::setw(40) << __func__ << " " << oss_msg.str() << "\n"; \
            std::cout << oss_final.str() << std::flush;                                                                           \
        }                                                                                                                         \
    } while (0)

#define EESTV_LOG_TRACE(message)                                                                                                  \
    do                                                                                                                            \
    {                                                                                                                             \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Trace))                                                          \
        {                                                                                                                         \
            constexpr const char* relative_path = eestv::logging::get_relative_path(__FILE__, EESTV_SOURCE_DIR);                  \
            std::ostringstream oss_msg;                                                                                           \
            oss_msg << message;                                                                                                   \
            std::ostringstream oss_location;                                                                                      \
            oss_location << relative_path << ":" << __LINE__;                                                                     \
            std::ostringstream oss_final;                                                                                         \
            oss_final << std::left << std::setw(7) << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Trace) << " "  \
                      << std::setw(55) << oss_location.str() << " " << std::setw(40) << __func__ << " " << oss_msg.str() << "\n"; \
            std::cout << oss_final.str() << std::flush;                                                                           \
        }                                                                                                                         \
    } while (0)
