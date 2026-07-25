#pragma once
#include <atomic>
#include <concepts>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace utils {
    /// @brief 单元素生产者-消费者同步控制器。
    /// 通过 m_is_ready 的翻转协调一对读写操作：
    ///   - m_is_ready == false → 生产者侧可操作（缓冲区空闲）
    ///   - m_is_ready == true  → 消费者侧可操作（数据就绪）
    ///
    /// 生产者和消费者共享同一个 condition_variable，toggle_ready_and_notify_all() 翻转
    /// m_is_ready 后使用 notify_all 唤醒所有线程。谓词不匹配的线程自动
    /// 回到睡眠，正确的线程继续执行。
    ///
    /// @note 不可拷贝/移动：内部持有 std::mutex 和 std::condition_variable。
    /// @note 线程安全：所有公开方法均为线程安全。
    class SyncController {
    protected:
        std::mutex m_mtx;                      ///< 保护 m_cv 及关联的等待-通知时序
        std::condition_variable m_cv;          ///< 生产者与消费者共享的条件变量
        std::atomic<bool> m_is_stop { false }; ///< 停止标志：为 true 时所有阻塞等待终止
        bool m_is_ready = false;               ///< 就绪标志：由 m_mtx 保护

    public:
        SyncController() = default;
        virtual ~SyncController() = default;

        SyncController(const SyncController&) = delete;
        SyncController& operator=(const SyncController&) = delete;
        SyncController(SyncController&&) = delete;
        SyncController& operator=(SyncController&&) = delete;

        /// @brief 将内部状态重置为初始值。
        /// @pre 在所有线程启动前调用，线程创建本身提供所需的同步。
        /// @post m_is_stop == false, m_is_ready == false
        void init() noexcept
        {
            m_is_stop.store(false, std::memory_order_relaxed);
            m_is_ready = false;
        }

        /// @brief 阻塞当前线程，直到被通知或停止。
        /// @param is_consumer true:  等待 m_is_ready == true （消费者侧）
        ///                    false: 等待 m_is_ready == false（生产者侧）
        /// @return true 表示目标状态就绪；false 表示等待被 stop() 中断。
        [[nodiscard]] bool wait_for(bool is_consumer)
        {
            std::unique_lock lock(m_mtx);
            m_cv.wait(lock, [&] { return m_is_stop.load(std::memory_order_relaxed) || (is_consumer ? m_is_ready : !m_is_ready); });
            return !m_is_stop.load(std::memory_order_relaxed);
        }

        /// @brief 阻塞当前线程，直到自定义谓词满足或停止。
        /// @tparam Predicate 返回可转换为 bool 的无参谓词
        /// @param pred 自定义谓词，在持有 m_mtx 时求值
        /// @return true 表示谓词满足；false 表示等待被 stop() 中断。
        template<class Predicate>
        requires std::predicate<Predicate&>
        [[nodiscard]] bool wait_for(Predicate&& pred)
        {
            std::unique_lock lock(m_mtx);
            m_cv.wait(lock, [&] { return m_is_stop.load(std::memory_order_relaxed) || std::invoke(pred); });
            return !m_is_stop.load(std::memory_order_relaxed);
        }

        /// @brief 发出全局停止信号并唤醒所有等待线程。
        /// @post m_is_stop == true，所有正阻塞在 wait_for 中的线程将返回。
        /// @note 使用 release 语义，与 is_stopped() 的 acquire 语义配对，
        ///       确保调用者在看到 m_is_stop == true 后可见 stop() 之前的所有写入。
        void stop()
        {
            {
                std::lock_guard lock(m_mtx);
                m_is_stop.store(true, std::memory_order_release);
            }
            m_cv.notify_all();
        }

        /// @brief 查询是否已收到停止信号。
        /// @retval true  stop() 已被调用
        /// @retval false 正常运行中
        /// @note 使用 acquire 语义，与 stop() 的 release 语义配对。
        bool is_stopped() const noexcept { return m_is_stop.load(std::memory_order_acquire); }

        /// @brief 翻转 m_is_ready 并唤醒所有等待线程。
        ///
        /// 生产者调用后 m_is_ready 翻转为 true，消费者被唤醒；
        /// 消费者调用后 m_is_ready 翻转为 false，生产者被唤醒。
        ///
        /// @attention 此处必须使用 notify_all 而非 notify_one：
        ///   生产者和消费者共享同一个条件变量且等待互斥的谓词。
        ///   若仅唤醒一个线程，可能唤醒错误类型（例如翻转为 true
        ///   时却唤醒了一个生产者），该线程谓词不满足回到睡眠，
        ///   而目标线程未被唤醒 → 永久阻塞。
        void toggle_ready_and_notify_all()
        {
            {
                std::lock_guard lock(m_mtx);
                m_is_ready = !m_is_ready;
            }
            m_cv.notify_all();
        }

        [[deprecated("use toggle_ready_and_notify_all()")]] void notify_one() { toggle_ready_and_notify_all(); }
    };
} // namespace utils
