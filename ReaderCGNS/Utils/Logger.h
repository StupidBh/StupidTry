#pragma once
#include "ReaderCGNS/ReaderCGNS.h"

#include <atomic>

#include "Logger/logger_formatter.hpp"

namespace reader_cgns {
    inline std::atomic<ReaderCGNS_LogCallback> g_log_callback = nullptr;

    inline void LogMessage(int level, const char* file, int line, const char* function, const std::string& message)
    {
        const auto callback = g_log_callback.load(std::memory_order_acquire);
        if (callback == nullptr) {
            return;
        }

        callback(level, file, line, function, message.c_str());
    }

    template<class... Args>
    void LogFormat(int level,
                   const char* file,
                   int line,
                   const char* function,
                   fmt::format_string<Args...> fmt_text,
                   Args&&... args)
    {
        try {
            LogMessage(level, file, line, function, fmt::format(fmt_text, std::forward<Args>(args)...));
        }
        catch (const std::exception& e) {
            LogMessage(level, file, line, function, fmt::format("ReaderCGNS log format failed: {}", e.what()));
        }
        catch (...) {
            LogMessage(level, file, line, function, "ReaderCGNS log format failed: unknown error.");
        }
    }

    inline void LogFormat(int level, const char* file, int line, const char* function, fmt::string_view message)
    {
        LogMessage(level, file, line, function, std::string(message.data(), message.size()));
    }
} // namespace reader_cgns

#define LOG_DEBUG(...) reader_cgns::LogFormat(READER_CGNS_LOG_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...)  reader_cgns::LogFormat(READER_CGNS_LOG_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...)  reader_cgns::LogFormat(READER_CGNS_LOG_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_ERROR(...) reader_cgns::LogFormat(READER_CGNS_LOG_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
