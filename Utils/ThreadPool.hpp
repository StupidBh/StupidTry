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
                            m_task_cv.wait(lock, [this, &st] { return m_stopped || !m_tasks.empty() || st.stop_requested(); });

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

            // 用 lambda + init-capture 转发，保留 move-only / 引用语义（std::bind 会衰减拷贝）
            auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
                [f = std::forward<Func>(f), ... a = std::forward<Args>(args)]() mutable -> return_type {
                    return std::invoke(std::move(f), std::move(a)...);
                });

            std::future<return_type> future = task_ptr->get_future();

            {
                std::scoped_lock lock(m_mutex);
                if (m_stopped) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                // 先入队，成功后再自增计数，避免 emplace 抛异常导致计数器泄漏
                m_tasks.emplace([task_ptr, this]() {
                    (*task_ptr)(); // 注意：异常会由 packaged_task 保存到 future
                    if (m_unfinished_tasks.fetch_sub(1) == 1) {
                        std::scoped_lock lk(m_mutex);
                        m_completion_cv.notify_all();
                    }
                });
                ++m_unfinished_tasks;
            }

            m_task_cv.notify_one();
            return future;
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
            {
                // 先置位 m_stopped 再等待，堵住关闭期间的新 enqueue，消除
                // “wait 完成后、置位前”窗口内入队的任务被丢弃 / future 永挂的竞态。
                std::scoped_lock lock(m_mutex);
                if (m_stopped) {
                    return;
                }
                m_stopped = true;
            }
            this->wait_for_completion();
            m_task_cv.notify_all();
            this->clear_workers();
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

                // 只扣减“尚未执行”的任务计数；正在执行中的任务不在队列里，
                // 会在自己的包装体内 fetch_sub 递减。直接清零会导致在途任务的
                // fetch_sub 把无符号计数下溢成 SIZE_MAX，使等待者永久挂起。
                std::queue<std::function<void()>> empty;
                m_unfinished_tasks.fetch_sub(m_tasks.size());
                std::swap(m_tasks, empty);
            }
            m_task_cv.notify_all();
            m_completion_cv.notify_all();
            this->clear_workers();
        }

    private:
        // 串行化 m_workers.clear()，避免并发 shutdown / shutdown_now 对同一 vector 重复析构
        void clear_workers()
        {
            std::call_once(m_clear_once, [this] { m_workers.clear(); });
        }

    private:
        std::vector<std::jthread> m_workers;       // 工作线程
        std::queue<std::function<void()>> m_tasks; // 任务队列

        std::atomic<size_t> m_unfinished_tasks;    // 未完成任务数
        bool m_stopped;                            // 停止标志位

        std::mutex m_mutex;                        // 任务队列互斥锁
        std::condition_variable m_task_cv;         // 新任务通知
        std::condition_variable m_completion_cv;   // 任务完成通知
        std::once_flag m_clear_once;               // 保证 m_workers 只清理一次
    };
} // namespace utils
