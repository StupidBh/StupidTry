#pragma once
#include "ReaderCGNS/ReaderCGNSTypes.hpp"

class ReaderCGNSLogGuard final {
public:
    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    ReaderCGNSLogGuard() noexcept;
    ~ReaderCGNSLogGuard() noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_active; }

private:
    static void LogCallback(void* context, ReaderCGNS::Logger::ReaderCGNS_LogLevel level, const char* file, int line, const char* message);

    bool m_active = false;
};
