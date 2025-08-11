#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threads) :
    m_stop(false),
    m_unfinished_tasks(0)
{
    m_workers.reserve(threads);
    for (size_t i = 0; i < threads; ++i) {
        m_workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_queue_mutex);
                    m_condition.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) {
                        return;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_stop = true;
    }
    m_condition.notify_all();

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::wait_for_completion()
{
    std::unique_lock<std::mutex> lock(m_queue_mutex);
    m_completion_condition.wait(lock, [this] { return m_unfinished_tasks == 0; });
}
