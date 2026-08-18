#pragma once
#include <concepts>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>

namespace utils {
    /// @brief 单元素生产者-消费者同步控制器。
    /// 通过 m_is_ready 的翻转协调一对读写操作：
    ///   - m_is_ready == false → 生产者侧可操作（缓冲区空闲）
    ///   - m_is_ready == true  → 消费者侧可操作（数据就绪）
    ///
    /// 生产者和消费者共享同一个 condition_variable_any。mark_ready() 和
    /// mark_consumed() 设置 m_is_ready 后使用 notify_all 唤醒所有线程。谓词不匹配的线程自动
    /// 回到睡眠，正确的线程继续执行。
    ///
    /// @note 不可拷贝/移动：内部持有 std::mutex 和 std::condition_variable_any。
    /// @note 除 init() 外的公开方法均为线程安全；init() 必须在工作线程启动前调用。
    class SyncController {
    public:
        enum class Side
        {
            producer,
            consumer
        };

        SyncController(const SyncController&) = delete;
        SyncController& operator=(const SyncController&) = delete;
        SyncController(SyncController&&) = delete;
        SyncController& operator=(SyncController&&) = delete;

        SyncController() = default;
        virtual ~SyncController() = default;

        /// @brief 将内部状态重置为初始值。
        /// @pre 在所有线程启动前调用，线程创建本身提供所需的同步。
        /// @post stop token 未被请求停止，m_is_ready == false
        void init()
        {
            this->m_stop_source = std::stop_source { };
            this->m_is_ready = false;
        }

        /// @brief 阻塞当前线程，直到被通知或停止。
        /// @param side consumer 等待 m_is_ready == true；producer 等待 m_is_ready == false
        /// @return true 表示目标状态就绪；false 表示等待被 stop() 中断。
        [[nodiscard]] bool wait_for(const Side side)
        {
            const std::stop_token stop_token = this->m_stop_source.get_token();
            std::unique_lock lock(this->m_mtx);
            this->m_cv.wait(lock, stop_token, [this, side] { return side == Side::consumer ? this->m_is_ready : !this->m_is_ready; });
            return !stop_token.stop_requested();
        }

        [[deprecated("use wait_for(Side)")]] [[nodiscard]] bool wait_for(const bool is_consumer)
        {
            return this->wait_for(is_consumer ? Side::consumer : Side::producer);
        }

        /// @brief 阻塞当前线程，直到自定义谓词满足或停止。
        /// @tparam Predicate 返回可转换为 bool 的无参谓词
        /// @param pred 自定义谓词，在持有 m_mtx 时求值
        /// @return true 表示谓词满足；false 表示等待被 stop() 中断。
        template<class Predicate>
        requires std::predicate<Predicate&>
        [[nodiscard]] bool wait_for(Predicate&& pred)
        {
            const std::stop_token stop_token = this->m_stop_source.get_token();
            std::unique_lock lock(this->m_mtx);
            this->m_cv.wait(lock, stop_token, [&pred] { return std::invoke(pred); });
            return !stop_token.stop_requested();
        }

        /// @brief 发出全局停止信号并唤醒所有等待线程。
        /// @post stop token 被请求停止，所有正阻塞在 wait_for 中的线程将返回。
        void stop()
        {
            this->m_stop_source.request_stop();
            this->m_cv.notify_all();
        }

        /// @brief 查询是否已收到停止信号。
        /// @retval true  stop() 已被调用
        /// @retval false 正常运行中
        bool is_stopped() const noexcept { return this->m_stop_source.stop_requested(); }

        [[nodiscard]] std::stop_token get_stop_token() const noexcept { return this->m_stop_source.get_token(); }

        /// @brief 将状态设为数据就绪并唤醒消费者。
        void mark_ready() { this->set_ready_and_notify_all(true); }

        /// @brief 将状态设为空闲并唤醒生产者。
        void mark_consumed() { this->set_ready_and_notify_all(false); }

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
        [[deprecated("use mark_ready() or mark_consumed()")]] void toggle_ready_and_notify_all() { this->toggle_ready_state_and_notify_all(); }

        [[deprecated("use mark_ready() or mark_consumed()")]] void notify_one() { this->toggle_ready_state_and_notify_all(); }

    protected:
        void set_ready_and_notify_all(const bool ready)
        {
            {
                std::lock_guard lock(this->m_mtx);
                this->m_is_ready = ready;
            }
            this->m_cv.notify_all();
        }

        void toggle_ready_state_and_notify_all()
        {
            {
                std::lock_guard lock(this->m_mtx);
                this->m_is_ready = !this->m_is_ready;
            }
            this->m_cv.notify_all();
        }

        std::mutex m_mtx;                 ///< 保护 m_cv 及关联的等待-通知时序
        std::condition_variable_any m_cv; ///< 生产者与消费者共享的条件变量
        std::stop_source m_stop_source;   ///< 可重置的协作停止源
        bool m_is_ready = false;          ///< 就绪标志：由 m_mtx 保护
    };
} // namespace utils
