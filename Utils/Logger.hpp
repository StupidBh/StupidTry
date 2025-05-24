#pragma once
#include "SingletonHolder.hpp"

#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ranges.h" // 启用容器格式化支持
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/daily_file_sink.h"

namespace _Logging_ {
    class Logger final : public asst::SingletonHolder<Logger> {
        DELETE_COPY_AND_MOVE(Logger);

        friend class asst::SingletonHolder<Logger>;
        Logger() = default;

    public:
        ~Logger() override = default;

        inline std::unique_ptr<spdlog::logger>& log()
        {
            if (m_log == nullptr) {
                this->InitLog(".", TARGET_NAME, false);
            }
            return this->m_log;
        }

        void InitLog(const std::string& work_path, const std::string& file_name, bool verbose = false)
        {
            constexpr auto log_fmt =
#ifdef _DEBUG
                // [年-月-日] [时-分-秒-毫秒] [P:进程ID] [日志等级] [文件名:行号]
                "[%Y-%m-%d %H:%M:%S.%e] [P:%P] [%^%l%$] [%s:%#] %v";
#else
                "[%Y-%m-%d %H:%M:%S.%e] [P:%P] [%^%l%$] %v";
#endif

            // 终端回显日志消息
            std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink =
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(log_fmt);

            // 写入外部文件的日志消息
            std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink =
                std::make_shared<spdlog::sinks::daily_file_sink_mt>(fmt::format("{}/logs/{}.log", work_path, file_name), 0, 0);
            file_sink->set_pattern(log_fmt);

            // 整合日志系统
            spdlog::sinks_init_list log_sinks_list = { console_sink, file_sink };
            m_log = std::make_unique<spdlog::logger>("stupid-log", log_sinks_list);
            m_log->set_level(spdlog::level::trace);

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

            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);
        }

    private:
        std::unique_ptr<spdlog::logger> m_log = nullptr;
    };
}

#ifdef _DEBUG
#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::err, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(_Logging_::Logger::get_instance().log(), spdlog::level::debug, __VA_ARGS__)
#else
#define LOG_INFO(...)  _Logging_::Logger::get_instance().log()->info(__VA_ARGS__)
#define LOG_WARN(...)  _Logging_::Logger::get_instance().log()->warn(__VA_ARGS__)
#define LOG_ERROR(...) _Logging_::Logger::get_instance().log()->error(__VA_ARGS__)
#define LOG_DEBUG(...) _Logging_::Logger::get_instance().log()->debug(__VA_ARGS__)
#endif // _DEBUG

