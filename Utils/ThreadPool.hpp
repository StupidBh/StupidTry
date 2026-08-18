#pragma once
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <future>
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
        using Task = std::move_only_function<void()>;

        enum class State
        {
            running,
            draining,
            cancelling,
            stopped
        };

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

            ~TaskExecutionScope() noexcept { s_current_scope = this->m_previous_scope; }

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

        class TaskCompletionScope {
        public:
            explicit TaskCompletionScope(ThreadPool* pool) noexcept :
                m_pool(pool)
            {
            }

            ~TaskCompletionScope() noexcept { this->m_pool->finish_task(); }

            TaskCompletionScope(const TaskCompletionScope&) = delete;
            TaskCompletionScope& operator=(const TaskCompletionScope&) = delete;

        private:
            ThreadPool* m_pool;
        };

    public:
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        explicit ThreadPool(std::size_t thread_count) :
            m_unfinished_tasks(0),
            m_run_inline(thread_count == 0)
        {
            try {
                this->m_workers.reserve(thread_count);
                for (std::size_t i = 0; i < thread_count; ++i) {
                    this->m_workers.emplace_back([this](std::stop_token st) {
                        while (true) {
                            Task task;
                            {
                                std::unique_lock lock(this->m_mutex);
                                const bool ready =
                                    this->m_task_cv.wait(lock, st, [this] { return this->m_state != State::running || !this->m_tasks.empty(); });

                                if (!ready || st.stop_requested() || this->m_tasks.empty()) {
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
                    this->m_state = State::cancelling;
                }
                for (auto& worker : this->m_workers) {
                    worker.request_stop();
                }
                this->m_task_cv.notify_all();
                this->m_workers.clear();
                throw;
            }
        }

        ~ThreadPool() noexcept
        {
            try {
                this->shutdown();
            }
            catch (...) {
                std::terminate();
            }
        }

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
            std::packaged_task<return_type()> packaged_task([f = std::forward<Func>(f), ... a = std::forward<Args>(args)]() mutable -> return_type {
                return std::invoke(std::move(f), std::move(a)...);
            });

            std::future<return_type> future = packaged_task.get_future();
            Task task([task = std::move(packaged_task), this]() mutable {
                TaskExecutionScope execution_scope(this);
                TaskCompletionScope completion_scope(this);
                task();
            });
            bool run_inline = false;

            {
                std::scoped_lock lock(this->m_mutex);
                if (this->m_state != State::running) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }

                if (this->m_run_inline) {
                    ++this->m_unfinished_tasks;
                    run_inline = true;
                }
                else {
                    // 先入队，成功后再自增计数，避免 emplace 抛异常导致计数器泄漏
                    this->m_tasks.emplace(std::move(task));
                    ++this->m_unfinished_tasks;
                }
            }

            if (run_inline) {
                task();
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
                // 先切换状态再等待，堵住关闭期间的新 enqueue，消除
                // “wait 完成后、置位前”窗口内入队的任务被丢弃 / future 永挂的竞态。
                std::unique_lock lock(this->m_mutex);
                if (this->m_state == State::stopped) {
                    return;
                }
                if (this->m_state != State::running) {
                    this->m_shutdown_cv.wait(lock, [this] { return this->m_state == State::stopped; });
                    return;
                }
                this->m_state = State::draining;
            }
            this->m_task_cv.notify_all();
            this->wait_for_completion();
            this->clear_workers();
            this->mark_stopped();
        }

        /**
         * @brief 立即关闭，丢弃未执行的任务
         * @throws std::logic_error 当前线程正在执行此线程池的任务
         */
        void shutdown_now()
        {
            this->throw_if_called_from_task("shutdown_now cannot be called from a task running in the same ThreadPool");
            {
                std::unique_lock lock(this->m_mutex);
                if (this->m_state == State::stopped) {
                    return;
                }
                if (this->m_state != State::running) {
                    this->m_shutdown_cv.wait(lock, [this] { return this->m_state == State::stopped; });
                    return;
                }
                this->m_state = State::cancelling;

                // 只扣减“尚未执行”的任务计数；正在执行中的任务不在队列里，
                // 会在自己的包装体内递减。直接清零会破坏在途任务的计数。
                std::queue<Task> empty;
                this->m_unfinished_tasks -= this->m_tasks.size();
                std::swap(this->m_tasks, empty);
            }
            this->m_task_cv.notify_all();
            this->m_completion_cv.notify_all();
            this->clear_workers();
            this->mark_stopped();
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

        void clear_workers() { this->m_workers.clear(); }

        void mark_stopped()
        {
            {
                std::lock_guard lock(this->m_mutex);
                this->m_state = State::stopped;
            }
            this->m_shutdown_cv.notify_all();
        }

        std::vector<std::jthread> m_workers;     // 工作线程
        std::queue<Task> m_tasks;                // 任务队列

        std::size_t m_unfinished_tasks;          // 未完成任务数，由 m_mutex 保护
        const bool m_run_inline;                 // thread_count == 0: execute enqueue tasks inline
        State m_state = State::running;          // 生命周期状态，由 m_mutex 保护

        std::mutex m_mutex;                      // 任务队列互斥锁
        std::condition_variable_any m_task_cv;   // 新任务或停止通知
        std::condition_variable m_completion_cv; // 任务完成通知
        std::condition_variable m_shutdown_cv;   // 关闭完成通知
    };
} // namespace utils
