#pragma once
#include "ReaderAPI/ReaderApiBase.h"

#include <atomic>
#include <format>
#include <mutex>
#include <source_location>
#include <string_view>
#include <utility>

class LogDispatcher final {
public:
    LogDispatcher() = default;
    ~LogDispatcher() noexcept;

    LogDispatcher(const LogDispatcher&) = delete;
    LogDispatcher& operator=(const LogDispatcher&) = delete;
    LogDispatcher(LogDispatcher&&) = delete;
    LogDispatcher& operator=(LogDispatcher&&) = delete;

    bool SetCallback(ReaderAPI::Logger::LogCallback callback, void* context) noexcept;
    bool ClearCallback() noexcept;

    template<class... Args>
    void FormatAndDispatch(ReaderAPI::Logger::LogLevel level,
                           const char* file,
                           int line,
                           std::format_string<Args...> fmt_text,
                           Args&&... args) noexcept
    {
        if (!this->IsDispatchEnabled(level)) {
            return;
        }

        try {
            const std::string message = std::format(fmt_text, std::forward<Args>(args)...);
            this->Dispatch(level, file, line, message.c_str());
        }
        catch (...) {
            this->Dispatch(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    void FormatAndDispatch(ReaderAPI::Logger::LogLevel level, const char* file, int line, std::string_view message) noexcept
    {
        if (!this->IsDispatchEnabled(level)) {
            return;
        }

        try {
            const std::string owned_message(message);
            this->Dispatch(level, file, line, owned_message.c_str());
        }
        catch (...) {
            this->Dispatch(level, file, line, "ReaderCGNS log formatting failed.");
        }
    }

    int HandleCgnsStatus(int status, std::string_view call, std::source_location location) noexcept;

private:
    bool IsDispatchEnabled(ReaderAPI::Logger::LogLevel level) const noexcept;
    void Dispatch(ReaderAPI::Logger::LogLevel level, const char* file, int line, const char* message) noexcept;

    std::mutex m_state_mutex;
    std::mutex m_update_mutex;
    std::atomic_size_t m_active_callbacks { 0 };
    std::atomic_bool m_enabled { false };
    ReaderAPI::Logger::LogCallback m_callback = nullptr;
    void* m_context = nullptr;
};

#define LOG_DEBUG(...) this->GetLogDispatcher().FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  this->GetLogDispatcher().FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  this->GetLogDispatcher().FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) this->GetLogDispatcher().FormatAndDispatch(ReaderAPI::Logger::READER_CGNS_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define CGNS_LOG_CALL(STATUS) this->GetLogDispatcher().HandleCgnsStatus((STATUS), #STATUS, std::source_location::current())
