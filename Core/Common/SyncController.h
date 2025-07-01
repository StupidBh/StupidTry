#pragma once
#include <mutex>
#include <atomic>
#include <condition_variable>

class SyncController {
protected:
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic_bool m_is_stop { false };
    std::atomic_bool m_is_ready { false };

public:
    SyncController() = default;
    virtual ~SyncController() = default;

    void init() noexcept
    {
        m_is_stop.store(false, std::memory_order_release);
        m_is_ready.store(false, std::memory_order_release);
    }

    void wait_for(bool is_consumer)
    {
        std::unique_lock lock(m_mtx);
        m_cv.wait(lock, [&] { return m_is_stop.load() || (is_consumer ? m_is_ready.load() : !m_is_ready.load()); });
    }

    template<class _Ty>
    requires(!std::is_same_v<_Ty, bool>)
    inline void wait_for(_Ty&& pred)
    {
        std::unique_lock lock(m_mtx);
        m_cv.wait(lock, [&] { return m_is_stop.load() || pred(); });
    }

    void stop() noexcept
    {
        std::lock_guard lock(m_mtx);
        m_is_stop.store(true, std::memory_order_release);
        m_cv.notify_all();
    }

    bool is_stopped() noexcept { return m_is_stop.load(std::memory_order_acquire); }

    void notify_one() noexcept
    {
        std::lock_guard lock(m_mtx);
        m_is_ready.store(m_is_ready.load() ? false : true);
        m_cv.notify_one();
    }
};
