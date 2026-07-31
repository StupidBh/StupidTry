#pragma once
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace utils {
    class ThreadPool {
        class TaskExecutionScope;
        inline static thread_local TaskExecutionScope* s_current_scope = nullptr;

        class TaskExecutionScope {
        public:
            explicit TaskExecutionScope(ThreadPool* pool) noexcept :
                m_pool(pool),
                m_previous_scope(s_current_scope)
            {
                s_current_scope = this;
            }

            ~TaskExecutionScope() { s_current_scope = this->m_previous_scope; }

            static bool Contains(const ThreadPool* pool) noexcept
            {
                for (auto* scope = s_current_scope; scope != nullptr; scope = scope->m_previous_scope) {
                    if (scope->m_pool == pool) {
                        return true;
                    }
                }
                return false;
            }

        private:
            ThreadPool* m_pool;
            TaskExecutionScope* m_previous_scope;
        };

    public:
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        explicit ThreadPool(std::size_t thread_count) :
            m_unfinished_tasks(0),
            m_stopped(false),
            m_run_inline(thread_count == 0)
        {
            try {
                this->m_workers.reserve(thread_count);
                for (std::size_t i = 0; i < thread_count; ++i) {
                    this->m_workers.emplace_back([this](std::stop_token st) {
                        while (!st.stop_requested()) {
                            std::function<void()> task;
                            {
                                std::unique_lock lock(this->m_mutex);
                                this->m_task_cv.wait(lock, [this, &st] { return this->m_stopped || !this->m_tasks.empty() || st.stop_requested(); });

                                if ((this->m_stopped && this->m_tasks.empty()) || st.stop_requested()) {
                                    return;
                                }

                                task = std::move(this->m_tasks.front());
                                this->m_tasks.pop();
                            }

                            if (task) {
                                task();
                            }
                        }
                    });
                }
            }
            catch (...) {
                {
                    std::lock_guard lock(this->m_mutex);
                    this->m_stopped = true;
                }
                for (auto& worker : this->m_workers) {
                    worker.request_stop();
                }
                this->m_task_cv.notify_all();
                this->m_workers.clear();
                throw;
            }
        }

        ~ThreadPool() { this->shutdown(); }

        /**
         * @brief 提交任务到线程池
         * @tparam Func 可调用对象类型
         * @tparam Args 参数类型
         * @param f 任务函数
         * @param args 参数
         * @return std::future<任务返回类型>
         */
        template<class Func, class... Args>
        requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
        auto enqueue(Func&& f, Args&&... args) -> std::future<std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>>
        {
            using return_type = std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

            // 与 std::thread 一致：任务和参数衰减复制进任务对象；引用语义通过 std::ref 显式表达。
            auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(
                [f = std::forward<Func>(f), ... a = std::forward<Args>(args)]() mutable -> return_type {
                    return std::invoke(std::move(f), std::move(a)...);
                });

            std::future<return_type> future = task_ptr->get_future();
            bool run_inline = false;

            {
                std::scoped_lock lock(this->m_mutex);
                if (this->m_stopped) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                if (this->m_run_inline) {
                    ++this->m_unfinished_tasks;
                    run_inline = true;
                }
                else {
                    // 先入队，成功后再自增计数，避免 emplace 抛异常导致计数器泄漏
                    this->m_tasks.emplace([task_ptr, this] {
                        TaskExecutionScope execution_scope(this);
                        (*task_ptr)(); // 注意：异常会由 packaged_task 保存到 future
                        this->finish_task();
                    });
                    ++this->m_unfinished_tasks;
                }
            }

            if (run_inline) {
                {
                    TaskExecutionScope execution_scope(this);
                    (*task_ptr)();
                }
                this->finish_task();
                return future;
            }

            this->m_task_cv.notify_one();
            return future;
        }

        /**
         * @brief 阻塞等待所有任务完成
         * @throws std::logic_error 当前线程正在执行此线程池的任务
         */
        void wait_for_completion()
        {
            this->throw_if_called_from_task("wait_for_completion cannot be called from a task running in the same ThreadPool");
            std::unique_lock lock(this->m_mutex);
            this->m_completion_cv.wait(lock, [this] { return this->m_unfinished_tasks == 0; });
        }

        /**
         * @brief 带超时的等待
         * @tparam Rep 时间单位
         * @tparam Period 周期
         * @param timeout 等待时间
         * @return true: 所有任务完成，false: 超时
         * @throws std::logic_error 当前线程正在执行此线程池的任务
         */
        template<class Rep, class Period>
        bool wait_for_completion(const std::chrono::duration<Rep, Period>& timeout)
        {
            this->throw_if_called_from_task("wait_for_completion cannot be called from a task running in the same ThreadPool");
            std::unique_lock lock(this->m_mutex);
            return this->m_completion_cv.wait_for(lock, timeout, [this] { return this->m_unfinished_tasks == 0; });
        }

        /**
         * @brief 优雅关闭，等待任务完成
         * @throws std::logic_error 当前线程正在执行此线程池的任务
         */
        void shutdown()
        {
            this->throw_if_called_from_task("shutdown cannot be called from a task running in the same ThreadPool");
            {
                // 先置位 m_stopped 再等待，堵住关闭期间的新 enqueue，消除
                // “wait 完成后、置位前”窗口内入队的任务被丢弃 / future 永挂的竞态。
                std::scoped_lock lock(this->m_mutex);
                if (this->m_stopped) {
                    return;
                }
                this->m_stopped = true;
            }
            this->wait_for_completion();
            this->m_task_cv.notify_all();
            this->clear_workers();
        }

        /**
         * @brief 立即关闭，丢弃未执行的任务
         * @throws std::logic_error 当前线程正在执行此线程池的任务
         */
        void shutdown_now()
        {
            this->throw_if_called_from_task("shutdown_now cannot be called from a task running in the same ThreadPool");
            {
                std::scoped_lock lock(this->m_mutex);
                if (this->m_stopped) {
                    return;
                }
                m_stopped = true;

                // 只扣减“尚未执行”的任务计数；正在执行中的任务不在队列里，
                // 会在自己的包装体内递减。直接清零会破坏在途任务的计数。
                std::queue<std::function<void()>> empty;
                this->m_unfinished_tasks -= this->m_tasks.size();
                std::swap(this->m_tasks, empty);
            }
            this->m_task_cv.notify_all();
            this->m_completion_cv.notify_all();
            this->clear_workers();
        }

    private:
        void throw_if_called_from_task(const char* message) const
        {
            if (TaskExecutionScope::Contains(this)) {
                throw std::logic_error(message);
            }
        }

        void finish_task() noexcept
        {
            bool all_finished = false;
            {
                std::lock_guard lock(this->m_mutex);
                --this->m_unfinished_tasks;
                all_finished = this->m_unfinished_tasks == 0;
            }
            if (all_finished) {
                this->m_completion_cv.notify_all();
            }
        }

        // 串行化 m_workers.clear()，避免并发 shutdown / shutdown_now 对同一 vector 重复析构
        void clear_workers()
        {
            std::call_once(this->m_clear_once, [this] { this->m_workers.clear(); });
        }

        std::vector<std::jthread> m_workers;       // 工作线程
        std::queue<std::function<void()>> m_tasks; // 任务队列

        std::size_t m_unfinished_tasks;            // 未完成任务数，由 m_mutex 保护
        bool m_stopped;                            // 停止标志位
        const bool m_run_inline;                   // thread_count == 0: execute enqueue tasks inline

        std::mutex m_mutex;                        // 任务队列互斥锁
        std::condition_variable m_task_cv;         // 新任务通知
        std::condition_variable m_completion_cv;   // 任务完成通知
        std::once_flag m_clear_once;               // 保证 m_workers 只清理一次
    };
} // namespace utils
