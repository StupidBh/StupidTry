#pragma once

#include "ReaderCGNS/ReaderCGNSTypes.hpp"

#include <memory>

namespace spdlog {
    class logger;
}

class ReaderCGNSLogGuard final {
public:
    [[nodiscard]] static std::unique_ptr<ReaderCGNSLogGuard> Create(std::shared_ptr<spdlog::logger> logger) noexcept;

    ReaderCGNSLogGuard(const ReaderCGNSLogGuard&) = delete;
    ReaderCGNSLogGuard& operator=(const ReaderCGNSLogGuard&) = delete;

    ~ReaderCGNSLogGuard() noexcept;

private:
    explicit ReaderCGNSLogGuard(std::shared_ptr<spdlog::logger> logger) noexcept;

    static void LogCallback(void* context, ReaderCGNS::Logger::ReaderCGNS_LogLevel level, const char* file, int line, const char* message);

    std::shared_ptr<spdlog::logger> m_logger;
    bool m_active = false;
};
