#pragma once
#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>
#include <string_view>

namespace utils {
    template<typename Duration = std::chrono::duration<double>>
    class ScopedTimer {
        using OutputCallback = std::function<void(std::string_view)>;

        ScopedTimer(const ScopedTimer&) = delete;
        ScopedTimer& operator=(const ScopedTimer&) = delete;
        ScopedTimer(ScopedTimer&&) = delete;
        ScopedTimer& operator=(ScopedTimer&&) = delete;

    public:
        explicit ScopedTimer(std::string_view name = "", OutputCallback callback = nullptr) noexcept :
            m_name(name),
            m_running(true),
            m_start(std::chrono::steady_clock::now()),
            m_callback(std::move(callback))
        {
        }

        ~ScopedTimer()
        {
            if (this->m_running) {
                this->stop();
            }
        }

        void stop()
        {
            if (!this->m_running) {
                return;
            }
            this->m_end_time = std::chrono::steady_clock::now();
            Duration elapsed = this->m_end_time - this->m_start;
            this->m_running = false;

            std::string msg;
            if (!this->m_name.empty()) {
                msg = std::format("[{}] Execution time: {:.3f}s", this->m_name, elapsed.count());
            }
            else {
                msg = std::format("Execution time: {:.3f}s", elapsed.count());
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

        Duration elapsed() const noexcept
        {
            auto now = this->m_running ? std::chrono::steady_clock::now() : this->m_end_time;
            return Duration(now - this->m_start);
        }

    private:
        bool m_running;
        const std::string m_name;
        const std::chrono::steady_clock::time_point m_start;
        std::chrono::steady_clock::time_point m_end_time;
        OutputCallback m_callback;
    };
}
