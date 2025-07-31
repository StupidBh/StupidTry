#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    void wait_for_completion();

    template<class Func, class... Args>
    void enqueue(Func&& f, Args&&... args)
    {
        auto task = std::bind(std::forward<Func>(f), std::forward<Args>(args)...);
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (m_stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            m_tasks.emplace([this, task]() {
                task();
                {
                    std::lock_guard<std::mutex> lock(m_queue_mutex);
                    --m_unfinished_tasks;
                    if (m_unfinished_tasks == 0) {
                        m_completion_condition.notify_all();
                    }
                }
                m_completion_condition.notify_one();
            });
            ++m_unfinished_tasks;
        }
        m_condition.notify_one();
    }

private:
    std::vector<std::thread> m_workers;             // 存储工作线程
    std::queue<std::function<void()>> m_tasks;      // 任务队列，存储要执行的任务
    size_t m_unfinished_tasks;                      // 未完成任务计数器

    bool m_stop;                                    // 停止标志，用于控制线程池的生命周期
    std::mutex m_queue_mutex;                       // 互斥锁，保护任务队列和计数器
    std::condition_variable m_condition;            // 条件变量，用于任务的同步
    std::condition_variable m_completion_condition; // 条件变量，用于等待所有任务完成
};

