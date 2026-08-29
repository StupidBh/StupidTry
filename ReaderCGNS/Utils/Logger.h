#pragma once
#include "ReaderAPI/ReaderCGNS.h"

#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace ReaderAPI::Logger {
    // Snapshot check used before formatting; Dispatch performs the definitive concurrent recheck.
    bool IsDispatchEnabled(LogLevel level) noexcept;
    void Dispatch(LogLevel level, const char* file, int line, const char* message) noexcept;

    template<class... Args>
    void FormatAndDispatch(LogLevel level, const char* file, int line, std::format_string<Args...> fmt_text, Args&&... args) noexcept
    {
        if (!IsDispatchEnabled(level)) {
            return;
        }

        try {
            const std::string message = std::format(fmt_text, std::forward<Args>(args)...);
            Dispatch(level, file, line, message.c_str());
        }
        catch (...) {
            Dispatch(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    inline void FormatAndDispatch(LogLevel level, const char* file, int line, std::string_view message) noexcept
    {
        if (!IsDispatchEnabled(level)) {
            return;
        }

        try {
            const std::string owned_message(message);
            Dispatch(level, file, line, owned_message.c_str());
        }
        catch (...) {
            Dispatch(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    int HandleCgnsStatus(int status, std::string_view call, std::source_location location) noexcept;
} // namespace ReaderAPI::Logger

#define LOG_DEBUG(...) ReaderAPI::Logger::FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  ReaderAPI::Logger::FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  ReaderAPI::Logger::FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) ReaderAPI::Logger::FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define CG_INFO(STATUS) ReaderAPI::Logger::HandleCgnsStatus((STATUS), #STATUS, std::source_location::current())
