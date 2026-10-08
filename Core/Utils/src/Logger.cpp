#include "Logger.h"

#include <exception>
#include <iostream>
#include <utility>
#include <vector>

#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

fmt::format_context::iterator fmt::formatter<std::filesystem::path>::format(const std::filesystem::path& msg, fmt::format_context& ctx) const
{
    return fmt::format_to(ctx.out(), "{}", msg.string());
}

Logger::Logger()
{
    if (spdlog::thread_pool() == nullptr) {
        spdlog::init_thread_pool(32768, 1);
    }
}

Logger::~Logger()
{
    // Drain the managed logging thread pool before source strings are destroyed.
    spdlog::shutdown();
}

const char* Logger::InternSourceFile(const char* file)
{
    if (file == nullptr || *file == '\0') {
        return "";
    }

    const std::string_view path(file);
    std::lock_guard lock(this->m_source_mutex);
    if (const auto iter = this->m_source_files.find(path); iter != this->m_source_files.end()) {
        return iter->c_str();
    }
    return this->m_source_files.emplace(path).first->c_str();
}

void Logger::InitLog(const std::filesystem::path& log_dir, const std::string& log_file_name, const spdlog::level::level_enum log_level)
{
    std::unique_lock lock(this->m_mutex);

    constexpr const char* log_fmt =
#ifndef NDEBUG
        // [年-月-日 时-分-秒-毫秒] [P:进程ID] [T:线程ID] [日志等级] [文件名:行号]
        "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] [%s:%#] %v";
#else
        "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] %v";
#endif

    // 终端回显日志消息
    const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    std::vector<spdlog::sink_ptr> log_sinks { console_sink };

    // 写入外部文件的日志消息
    try {
        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
        const std::filesystem::path log_path = log_dir / (log_file_name + ".log");
        log_sinks.emplace_back(std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path.string(), 0, 0, false, 30));
    }
    catch (const std::exception& e) {
        std::cerr << "File logging disabled: " << e.what() << '\n';
    }

    // 创建异步记录器
    const auto async_logger =
        std::make_shared<spdlog::async_logger>(log_file_name, log_sinks.begin(), log_sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    async_logger->set_pattern(log_fmt);
    async_logger->set_level(log_level);
    async_logger->flush_on(spdlog::level::trace);
    async_logger->set_error_handler([](const std::string& msg) { std::cerr << "[Logger ERROR] " << msg << std::endl; });

    spdlog::set_default_logger(async_logger);
}

void Logger::UpdateLog(std::shared_ptr<spdlog::logger> log)
{
    if (log == nullptr) {
        return;
    }
    std::unique_lock lock(this->m_mutex);
    spdlog::set_default_logger(std::move(log));
}

std::size_t Logger::SourceFileHash::operator()(const std::string_view value) const noexcept
{
    return std::hash<std::string_view> { }(value);
}
