#include "../ReaderCGNSLogGuard.h"

#include "Logger/logger.hpp"

namespace {
    using SetLogCallback = bool (*)(ReaderAPI::Logger::ReaderCGNS_LogCallback, void*) noexcept;
}

ReaderCGNSLogGuard::ReaderCGNSLogGuard(const HMODULE module) noexcept
{
    if (module == nullptr) {
        return;
    }

    const auto set_log_callback = reinterpret_cast<SetLogCallback>(GetProcAddress(module, "SetLogCallback"));
    this->m_clear_log_callback = reinterpret_cast<decltype(this->m_clear_log_callback)>(GetProcAddress(module, "ClearLogCallback"));
    if (set_log_callback == nullptr || this->m_clear_log_callback == nullptr) {
        this->m_clear_log_callback = nullptr;
        return;
    }

    this->m_active = set_log_callback(LogCallback, LOG.get());
}

ReaderCGNSLogGuard::~ReaderCGNSLogGuard() noexcept
{
    if (this->m_active && this->m_clear_log_callback != nullptr) {
        this->m_clear_log_callback();
    }
}

void ReaderCGNSLogGuard::LogCallback(void* context,
                                     const ReaderAPI::Logger::ReaderCGNS_LogLevel level,
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
    case ReaderAPI::Logger::READER_CGNS_LOG_TRACE   : spd_level = spdlog::level::trace; break;
    case ReaderAPI::Logger::READER_CGNS_LOG_DEBUG   : spd_level = spdlog::level::debug; break;
    case ReaderAPI::Logger::READER_CGNS_LOG_INFO    : spd_level = spdlog::level::info; break;
    case ReaderAPI::Logger::READER_CGNS_LOG_WARN    : spd_level = spdlog::level::warn; break;
    case ReaderAPI::Logger::READER_CGNS_LOG_ERROR   : spd_level = spdlog::level::err; break;
    case ReaderAPI::Logger::READER_CGNS_LOG_CRITICAL: spd_level = spdlog::level::critical; break;
    default                                         : spd_level = spdlog::level::info; break;
    }

#ifndef NDEBUG
    logger->log(spdlog::source_loc { file, line, "ReaderCGNS" }, spd_level, "[ReaderCGNS] {}", message);
#else
    logger->log(spd_level, "[ReaderCGNS] {}", message);
#endif
}
