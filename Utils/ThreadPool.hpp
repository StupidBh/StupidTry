#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace utils {
    class ThreadPool {
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

    public:
        explicit ThreadPool(size_t thread_count) :
            m_unfinished_tasks(0),
            m_stopped(false)
        {
            m_workers.reserve(thread_count);
            for (size_t i = 0; i < thread_count; ++i) {
                m_workers.emplace_back([this](std::stop_token st) {
                    while (!st.stop_requested()) {
                        std::function<void()> task;
                        {
                            std::unique_lock lock(m_mutex);
                            m_task_cv.wait(lock, [this, &st] {
                                return m_stopped || !m_tasks.empty() || st.stop_requested();
                            });

                            if ((m_stopped && m_tasks.empty()) || st.stop_requested()) {
                                return;
                            }

                            task = std::move(m_tasks.front());
                            m_tasks.pop();
                        }

                        if (task) {
                            task();
                        }
                    }
                });
            }
        }

        ~ThreadPool() { shutdown(); }

        /**
         * @brief 提交任务到线程池
         * @tparam Func 可调用对象类型
         * @tparam Args 参数类型
         * @param f 任务函数
         * @param args 参数
         * @return std::future<任务返回类型>
         */
        template<class Func, class... Args>
        requires std::invocable<Func, Args...>
        auto enqueue(Func&& f, Args&&... args) -> std::future<std::invoke_result_t<Func, Args...>>
        {
            using return_type = std::invoke_result_t<Func, Args...>;

            // 封装为 packaged_task，以支持 future
            auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<Func>(f), std::forward<Args>(args)...));

            {
                std::scoped_lock lock(m_mutex);
                if (m_stopped) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                ++m_unfinished_tasks;

                // 包装任务，更新计数器
                m_tasks.emplace([task_ptr, this]() {
                    (*task_ptr)(); // 注意：异常会由 packaged_task 保存到 future
                    if (m_unfinished_tasks.fetch_sub(1) == 1) {
                        std::scoped_lock lk(m_mutex);
                        m_completion_cv.notify_all();
                    }
                });
            }

            m_task_cv.notify_one();
            return task_ptr->get_future();
        }

        /**
         * @brief 阻塞等待所有任务完成
         */
        void wait_for_completion()
        {
            std::unique_lock lock(m_mutex);
            m_completion_cv.wait(lock, [this] { return m_unfinished_tasks.load() == 0; });
        }

        /**
         * @brief 带超时的等待
         * @tparam Rep 时间单位
         * @tparam Period 周期
         * @param timeout 等待时间
         * @return true: 所有任务完成，false: 超时
         */
        template<class Rep, class Period>
        bool wait_for_completion(const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock lock(m_mutex);
            return m_completion_cv.wait_for(lock, timeout, [this] { return m_unfinished_tasks.load() == 0; });
        }

        /**
         * @brief 优雅关闭，等待任务完成
         */
        void shutdown()
        {
            this->wait_for_completion();
            {
                std::scoped_lock lock(m_mutex);
                if (m_stopped) {
                    return;
                }
                m_stopped = true;
            }
            m_task_cv.notify_all();
            m_workers.clear();
        }

        /**
         * @brief 立即关闭，丢弃未执行的任务
         */
        void shutdown_now()
        {
            {
                std::scoped_lock lock(m_mutex);
                if (m_stopped) {
                    return;
                }
                m_stopped = true;

                std::queue<std::function<void()>> empty;
                std::swap(m_tasks, empty);
                m_unfinished_tasks = 0;
            }
            m_task_cv.notify_all();
            m_completion_cv.notify_all();
            m_workers.clear();
        }

    private:
        std::vector<std::jthread> m_workers;       // 工作线程
        std::queue<std::function<void()>> m_tasks; // 任务队列

        std::atomic<size_t> m_unfinished_tasks;    // 未完成任务数
        bool m_stopped;                            // 停止标志位

        std::mutex m_mutex;                        // 任务队列互斥锁
        std::condition_variable m_task_cv;         // 新任务通知
        std::condition_variable m_completion_cv;   // 任务完成通知
    };
}
