#pragma once
#include "LoggerFramework.h"
#include "log_formatter.hpp"

#include <shared_mutex>

#include "SingletonHolder.hpp"

#include "spdlog/async.h"
#include "spdlog/fmt/ranges.h"

namespace dylog {
    class LOG_EXPORT_API std::shared_mutex;

    class LOG_EXPORT_API Logger final : public utils::SingletonHolder<Logger> {
        SINGLETON_CLASS(Logger);
        std::shared_mutex m_mutex;

        Logger()
        {
            spdlog::init_thread_pool(32768, 2);

#ifdef _WIN32
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
#endif
        }

    public:
        ~Logger() override = default;

        void InitLog(const std::filesystem::path& work_dir, const std::string& log_file_name, bool verbose = false)
        {
            std::unique_lock lock(this->m_mutex);

            constexpr const char* log_fmt =
#ifdef _DEBUG
                // [年-月-日 时-分-秒-毫秒] [P:进程ID] [T:线程ID] [日志等级] [文件名:行号]
                "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] [%s:%#] %v";
#else
                "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] %v";
#endif

            // 终端回显日志消息
            std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            spdlog::sinks_init_list log_sinks_list = { console_sink };

            // 写入外部文件的日志消息
            std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink = nullptr;
            try {
                std::filesystem::path log_dir = work_dir / "logs";
                if (!std::filesystem::exists(log_dir)) {
                    std::filesystem::create_directories(log_dir);
                }

                std::filesystem::path log_path = log_dir / (log_file_name + ".log");
                file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path.string(), 0, 0, false, 30);

                log_sinks_list = { console_sink, file_sink };
            }
            catch (...) {
                std::cerr << "The log file creation failed. Roll back to the terminal!\n";
            }

            // 创建异步记录器
            auto async_logger = std::make_shared<spdlog::async_logger>(
                log_file_name,
                log_sinks_list,
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            async_logger->set_pattern(log_fmt);
            async_logger->set_level(verbose ? spdlog::level::trace : spdlog::level::info);
            async_logger->flush_on(spdlog::level::trace);
            async_logger->set_error_handler(
                [](const std::string& msg) { std::cerr << "[Logger ERROR] " << msg << std::endl; });

            spdlog::set_default_logger(async_logger);
        }

        void UpdateLog(std::shared_ptr<spdlog::logger> log)
        {
            std::unique_lock lock(this->m_mutex);
            spdlog::set_default_logger(log);
        }
    };
}

#define LOG spdlog::default_logger()

#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::warn, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::debug, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(spdlog::default_logger(), spdlog::level::err, __VA_ARGS__)
