#pragma once
#include "ReaderCGNS/ReaderCGNS.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace ReaderCGNS::Logger {
    bool ShouldLog(ReaderCGNS_LogLevel level) noexcept;
    void LogMessage(ReaderCGNS_LogLevel level, const char* file, int line, const char* message) noexcept;

    template<class... Args>
    void LogFormat(ReaderCGNS_LogLevel level, const char* file, int line, std::format_string<Args...> fmt_text, Args&&... args) noexcept
    {
        if (!ShouldLog(level)) {
            return;
        }

        try {
            const std::string message = std::format(fmt_text, std::forward<Args>(args)...);
            LogMessage(level, file, line, message.c_str());
        }
        catch (...) {
            LogMessage(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    inline void LogFormat(ReaderCGNS_LogLevel level, const char* file, int line, std::string_view message) noexcept
    {
        if (!ShouldLog(level)) {
            return;
        }

        try {
            const std::string owned_message(message);
            LogMessage(level, file, line, owned_message.c_str());
        }
        catch (...) {
            LogMessage(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    int cgns_catch_msg(int status, const std::filesystem::path& file, int line);
} // namespace ReaderCGNS::Logger

#define LOG_DEBUG(...) ReaderCGNS::Logger::LogFormat(ReaderCGNS::Logger::READER_CGNS_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ReaderCGNS::Logger::LogFormat(ReaderCGNS::Logger::READER_CGNS_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ReaderCGNS::Logger::LogFormat(ReaderCGNS::Logger::READER_CGNS_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ReaderCGNS::Logger::LogFormat(ReaderCGNS::Logger::READER_CGNS_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define CG_INFO(STATUS) ReaderCGNS::Logger::cgns_catch_msg(STATUS, __FILE__, __LINE__)
