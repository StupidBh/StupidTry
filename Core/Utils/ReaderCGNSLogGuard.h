#pragma once
#include "ReaderAPI/ReaderCGNSTypes.hpp"

#include <windows.h>

class ReaderCGNSLogGuard final {
public:
    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    explicit ReaderCGNSLogGuard(HMODULE module) noexcept;
    ~ReaderCGNSLogGuard() noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_active; }

private:
    static void LogCallback(void* context, ReaderAPI::Logger::ReaderCGNS_LogLevel level, const char* file, int line, const char* message);

    bool (*m_clear_log_callback)() noexcept = nullptr;
    bool m_active = false;
};
