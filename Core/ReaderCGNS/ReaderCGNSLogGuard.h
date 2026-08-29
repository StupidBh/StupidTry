#pragma once
#include "ReaderAPI/ReaderCGNS.h"

#include <windows.h>
#include <functional>

class ReaderCGNSLogGuard final {
public:
    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    explicit ReaderCGNSLogGuard(HMODULE module) noexcept;
    ~ReaderCGNSLogGuard() noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_active; }

private:
    static void LogCallback(void* context, ReaderAPI::Logger::LogLevel level, const char* file, int line, const char* message);

    std::function<bool()> m_clear_log_callback;
    bool m_active = false;
};
