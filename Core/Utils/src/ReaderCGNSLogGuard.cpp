#include "ReaderCGNSLogGuard.h"

#include "ReaderCGNS/ReaderCGNS.h"
#include "spdlog/spdlog.h"

#include <new>
#include <string_view>
#include <utility>

std::unique_ptr<ReaderCGNSLogGuard> ReaderCGNSLogGuard::Create(std::shared_ptr<spdlog::logger> logger) noexcept
{
    if (logger == nullptr) {
        return nullptr;
    }

    auto guard = std::unique_ptr<ReaderCGNSLogGuard>(new (std::nothrow) ReaderCGNSLogGuard(std::move(logger)));
    if (guard == nullptr || !ReaderCGNS::Logger::SetLogCallback(LogCallback, guard->m_logger.get())) {
        return nullptr;
    }

    guard->m_active = true;
    return guard;
}

ReaderCGNSLogGuard::ReaderCGNSLogGuard(std::shared_ptr<spdlog::logger> logger) noexcept :
    m_logger(std::move(logger))
{
}

ReaderCGNSLogGuard::~ReaderCGNSLogGuard() noexcept
{
    if (this->m_active) {
        ReaderCGNS::Logger::ClearLogCallback();
    }
}

void ReaderCGNSLogGuard::LogCallback(void* context,
                                     const ReaderCGNS::Logger::ReaderCGNS_LogLevel level,
                                     const char* file,
                                     const int line,
                                     const char* message)
{
    auto* logger = static_cast<spdlog::logger*>(context);
    if (logger == nullptr) {
        return;
    }

    spdlog::level::level_enum spd_level;
    switch (level) {
    case ReaderCGNS::Logger::READER_CGNS_LOG_TRACE   : spd_level = spdlog::level::trace; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_DEBUG   : spd_level = spdlog::level::debug; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_INFO    : spd_level = spdlog::level::info; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_WARN    : spd_level = spdlog::level::warn; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_ERROR   : spd_level = spdlog::level::err; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_CRITICAL: spd_level = spdlog::level::critical; break;
    default                                          : spd_level = spdlog::level::info; break;
    }

#ifndef NDEBUG
    std::string_view source = file != nullptr ? std::string_view(file) : std::string_view("unknown");
    if (const auto separator = source.find_last_of("/\\"); separator != std::string_view::npos) {
        source.remove_prefix(separator + 1);
    }
    logger->log(spd_level, "[ReaderCGNS] [{}:{}] {}", source, line, message != nullptr ? message : "EmptyMsg");
#else
    logger->log(spd_level, "[ReaderCGNS] {}", message != nullptr ? message : "EmptyMsg");
#endif
}
