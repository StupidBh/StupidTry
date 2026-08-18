#pragma once

#include "ReaderCGNS/ReaderCGNSTypes.hpp"

namespace spdlog {
    class logger;
}

class ReaderCGNSLogGuard final {
public:
    explicit ReaderCGNSLogGuard(spdlog::logger* logger) noexcept;

    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_active; }

    ~ReaderCGNSLogGuard() noexcept;

private:
    static void LogCallback(void* context, ReaderCGNS::Logger::ReaderCGNS_LogLevel level, const char* file, int line, const char* message);

    bool m_active = false;
};
