#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ranges.h"

template<>
struct fmt::formatter<std::filesystem::path>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    fmt::format_context::iterator format(const std::filesystem::path& msg, fmt::format_context& ctx) const;
};

class Logger final {
    struct SourceFileHash
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept;
    };

    std::shared_mutex m_mutex;
    std::mutex m_source_mutex;
    std::unordered_set<std::string, SourceFileHash, std::equal_to<>> m_source_files;

public:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    // Returned paths remain immutable until this Logger is destroyed.
    [[nodiscard]] const char* InternSourceFile(const char* file);

    void InitLog(const std::filesystem::path& log_dir, const std::string& log_file_name, spdlog::level::level_enum log_level = spdlog::level::level_enum::info);
    void UpdateLog(std::shared_ptr<spdlog::logger> log);
};

#define LOG spdlog::default_logger()

#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::err, __VA_ARGS__)
