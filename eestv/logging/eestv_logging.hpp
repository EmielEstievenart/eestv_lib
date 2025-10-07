#pragma once

#include <iostream>
#include <sstream>

namespace eestv
{
namespace logging
{

enum class LogLevel
{
    Error = 0,
    Info  = 1,
    Debug = 2,
    Trace = 3
};

// Global log level variable
extern LogLevel current_log_level;

// Helper function to check if a log level should be printed
inline bool should_log(LogLevel level)
{
    return static_cast<int>(level) <= static_cast<int>(current_log_level);
}

// Helper function to get log level name
inline const char* get_log_level_name(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Error:
        return "ERROR";
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
#define EESTV_LOG_ERROR(message)                                                                                                     \
    do                                                                                                                               \
    {                                                                                                                                \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Error))                                                             \
        {                                                                                                                            \
            std::ostringstream oss;                                                                                                  \
            oss << "[" << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Error) << "] " << __func__ << ": " << message \
                << "\n";                                                                                                             \
            std::cout << oss.str();                                                                                                  \
        }                                                                                                                            \
    } while (0)

#define EESTV_LOG_INFO(message)                                                                                                     \
    do                                                                                                                              \
    {                                                                                                                               \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Info))                                                             \
        {                                                                                                                           \
            std::ostringstream oss;                                                                                                 \
            oss << "[" << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Info) << "] " << __func__ << ": " << message \
                << "\n";                                                                                                            \
            std::cout << oss.str();                                                                                                 \
        }                                                                                                                           \
    } while (0)

#define EESTV_LOG_DEBUG(message)                                                                                                     \
    do                                                                                                                               \
    {                                                                                                                                \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Debug))                                                             \
        {                                                                                                                            \
            std::ostringstream oss;                                                                                                  \
            oss << "[" << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Debug) << "] " << __func__ << ": " << message \
                << "\n";                                                                                                             \
            std::cout << oss.str();                                                                                                  \
        }                                                                                                                            \
    } while (0)

#define EESTV_LOG_TRACE(message)                                                                                                     \
    do                                                                                                                               \
    {                                                                                                                                \
        if (eestv::logging::should_log(eestv::logging::LogLevel::Trace))                                                             \
        {                                                                                                                            \
            std::ostringstream oss;                                                                                                  \
            oss << "[" << eestv::logging::get_log_level_name(eestv::logging::LogLevel::Trace) << "] " << __func__ << ": " << message \
                << "\n";                                                                                                             \
            std::cout << oss.str();                                                                                                  \
        }                                                                                                                            \
    } while (0)
