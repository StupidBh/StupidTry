#pragma once
#include "log_formatter.hpp"
#include "SingletonHolder.hpp"

#include "spdlog/async.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ranges.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

template class LOG_EXPORT_API std::shared_ptr<spdlog::async_logger>;

namespace _Logging_ {
    class LOG_EXPORT_API Logger final : public utils::SingletonHolder<Logger> {
        std::shared_ptr<spdlog::async_logger> m_log;
        mutable std::shared_mutex m_mutex;

        Logger()
        {
#ifdef _WIN32
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
#endif
            spdlog::init_thread_pool(32768, 1);
        }

    public:
        ~Logger() override
        {
            this->ShutDown();
            spdlog::shutdown();
        }

        std::shared_ptr<spdlog::async_logger> log()
        {
            {
                std::shared_lock lock(this->m_mutex);
                if (this->m_log) {
                    return this->m_log;
                }
            }

            if (!this->m_log) {
                this->InitLog(".", "default");
            }
            return this->m_log;
        }

        void InitLog(
            const std::filesystem::path& work_dir,
            const std::string& log_file_name,
            [[maybe_unused]] bool verbose = false)
        {
            std::unique_lock lock(this->m_mutex);

            static constexpr const char* log_fmt =
#ifdef _DEBUG
                // [年-月-日 时-分-秒-毫秒] [P:进程ID] [T:线程ID] [日志等级] [文件名:行号]
                "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] [%s:%#] %v";
#else
                "[%Y-%m-%d %H:%M:%S.%e] [P:%5P] [T:%5t] [%^%L%$] %v";
#endif

            spdlog::sinks_init_list log_sinks_list;

            // 终端回显日志消息
            std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(log_fmt);
            log_sinks_list = { console_sink };

#ifdef _DEBUG
            console_sink->set_level(spdlog::level::debug);
#else
            console_sink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
#endif // _DEBUG

            std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink;
            try {
                // 写入外部文件的日志消息
                std::filesystem::path log_dir = work_dir / "logs";
                if (!std::filesystem::exists(log_dir)) {
                    std::filesystem::create_directories(log_dir);
                }

                std::string log_filename = std::format("{}/{}.log", log_dir.string(), log_file_name);
                file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 0, 0, false, 30);
                file_sink->set_pattern(log_fmt);

#ifdef _DEBUG
                file_sink->set_level(spdlog::level::debug);
#else
                file_sink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
#endif // _DEBUG

                log_sinks_list = { console_sink, file_sink };
            }
            catch (...) {
                std::cerr << "The log file creation failed. Roll back to the terminal!" << std::endl;
            }

            // 创建异步记录器
            this->m_log = std::make_shared<spdlog::async_logger>(
                "stupid-log",
                log_sinks_list,
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            this->m_log->set_pattern(log_fmt);
            this->m_log->set_level(spdlog::level::trace);
            this->m_log->flush_on(spdlog::level::trace);
            this->m_log->set_error_handler(
                [](const std::string& msg) { std::cerr << "[*** Logger ERROR ***] " << msg << std::endl; });
        }

        void ShutDown()
        {
            std::unique_lock lock(this->m_mutex);
            if (this->m_log) {
                this->m_log->flush();
            }
        }

    private:
        SINGLETON_CLASS(Logger);
    };

#define LOG Logger::get_instance().log()
}

#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(_Logging_::LOG, spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(_Logging_::LOG, spdlog::level::warn, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(_Logging_::LOG, spdlog::level::debug, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(_Logging_::LOG, spdlog::level::err, __VA_ARGS__)
