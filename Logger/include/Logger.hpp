#pragma once
#include "pch.h"
#include "SingletonHolder.hpp"

namespace _Logging_ {
    class EXPORT_API Logger final : public asst::SingletonHolder<Logger> {
        std::shared_ptr<spdlog::async_logger> m_log;
        mutable std::shared_mutex m_mutex;

        Logger()
        {
            try {
                InitLog(".", "Before-Init", false);
            }
            catch (const spdlog::spdlog_ex& e) {
                std::cerr << "Logger initialization failed: " << e.what() << std::endl;
                std::abort();
            }
        }

    public:
        ~Logger() override { ShutDown(); }

        inline auto log()
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            return this->m_log;
        }

        void InitLog(const std::string& work_path, const std::string& file_name, bool verbose = false)
        {
            constexpr auto log_fmt =
#ifdef _DEBUG
                // [年-月-日] [时-分-秒-毫秒] [T:线程ID] [P:进程ID] [日志等级] [文件名:行号]
                "[%Y-%m-%d %H:%M:%S.%e] [T:%t] [P:%P] [%^%l%$] [%s:%#] %v";
#else
                "[%Y-%m-%d %H:%M:%S.%e] [T:%t] [P:%P] [%^%l%$] %v";
#endif

            // 创建日志目录
            auto log_dir = std::filesystem::path(work_path) / "logs";
            try {
                if (!std::filesystem::exists(log_dir)) {
                    std::filesystem::create_directories(log_dir);
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Failed to create log directory: " << e.what() << std::endl;
                std::abort();
            }
            catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }

#ifdef _WIN32
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
#endif

            // 终端回显日志消息
            std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(log_fmt);

            // 写入外部文件的日志消息
            std::string log_filename = fmt::format("{}/{}.log", log_dir.string(), file_name);
            std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink =
                std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 0, 0, false, 30);
            file_sink->set_pattern(log_fmt);

            // 创建异步记录器
            spdlog::init_thread_pool(32768, 2);
            spdlog::sinks_init_list log_sinks_list = { console_sink, file_sink };

            m_log = std::make_shared<spdlog::async_logger>(
                "app",
                log_sinks_list,
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            m_log->set_pattern(log_fmt);
            m_log->set_level(spdlog::level::trace);
            m_log->set_error_handler([](const std::string& msg) { std::cerr << "[*** Logger ERROR ***] " << msg << std::endl; });

#ifdef _DEBUG
            console_sink->set_level(spdlog::level::debug);
            file_sink->set_level(spdlog::level::debug);
#else
            console_sink->set_level(spdlog::level::info);
            file_sink->set_level(spdlog::level::info);

            if (verbose) {
                console_sink->set_level(spdlog::level::debug);
                file_sink->set_level(spdlog::level::debug);
            }
#endif
            m_log->flush_on(spdlog::level::trace);
        }

        void ShutDown()
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (m_log) {
                m_log->flush();
            }
        }

    private:
        friend class asst::SingletonHolder<Logger>;
        DELETE_COPY_AND_MOVE(Logger);
    };
}

#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::warn, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::debug, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::err, __VA_ARGS__)
