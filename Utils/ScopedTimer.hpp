#pragma once
#include <chrono>
#include <exception>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace utils {
    namespace detail {
        template<class T>
        inline constexpr bool is_duration_v = false;

        template<class Rep, class Period>
        inline constexpr bool is_duration_v<std::chrono::duration<Rep, Period>> = true;
    } // namespace detail

    template<typename Duration = std::chrono::duration<double>>
    requires detail::is_duration_v<Duration>
    class ScopedTimer {
        using OutputCallback = std::function<void(std::string_view)>;

    public:
        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;

        explicit ScopedTimer(std::string_view name = "", OutputCallback callback = nullptr) :
            m_name(name),
            m_running(true),
            m_start(std::chrono::steady_clock::now()),
            m_callback(std::move(callback))
        {
        }

        ~ScopedTimer() noexcept
        {
            // 仅在非栈展开时输出：stop() 的回调（如 spdlog）可能抛异常，
            // 若此刻已有异常在传播，二次抛出会触发 std::terminate。
            // 栈展开中宁可放弃这条计时日志，也不冒 terminate 的风险。
            if (this->m_running && std::uncaught_exceptions() == 0) {
                try {
                    this->stop();
                }
                catch (...) {
                    this->m_running = false;
                }
            }
        }

        void stop()
        {
            if (!this->m_running) {
                return;
            }
            this->m_end_time = std::chrono::steady_clock::now();
            const auto elapsed_seconds = std::chrono::duration<double>(this->m_end_time - this->m_start).count();
            this->m_running = false;

            std::string msg;
            if (!this->m_name.empty()) {
                msg = std::format("[{}] Execution time: {:.3f}s", this->m_name, elapsed_seconds);
            }
            else {
                msg = std::format("Execution time: {:.3f}s", elapsed_seconds);
            }

            if (this->m_callback) {
                this->m_callback(msg);
            }
            else {
                static std::mutex mtx;
                std::scoped_lock lock(mtx);
                std::cerr << msg << std::endl;
            }
        }

        [[nodiscard]] Duration elapsed() const noexcept
        {
            const auto now = this->m_running ? std::chrono::steady_clock::now() : this->m_end_time;
            return std::chrono::duration_cast<Duration>(now - this->m_start);
        }

    private:
        bool m_running;
        const std::string m_name;
        const std::chrono::steady_clock::time_point m_start;
        std::chrono::steady_clock::time_point m_end_time;
        OutputCallback m_callback;
    };
} // namespace utils
