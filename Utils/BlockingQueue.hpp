#pragma once
#include <concepts>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace utils {
    /// @brief 基于 std::queue 的线程安全阻塞队列。
    /// @tparam T 队列中传递的数据类型。
    ///
    /// BlockingQueue 用于典型的生产-消费场景：
    /// - Push() 在队列未满时写入数据，队列满时阻塞等待；
    /// - Pop() 在队列非空时取出数据，队列为空时阻塞等待；
    /// - Close() 表示生产端不再写入，消费者会在取尽已有数据后收到 std::nullopt。
    ///
    /// @note max_size == 0 表示无界队列；有界队列可以在消费较慢时向生产者施加背压。
    /// @pre 析构队列前，调用方必须确保没有线程仍在调用或等待此对象。
    template<std::move_constructible T>
    class BlockingQueue {
        BlockingQueue(const BlockingQueue&) = delete;
        BlockingQueue& operator=(const BlockingQueue&) = delete;
        BlockingQueue(BlockingQueue&&) = delete;
        BlockingQueue& operator=(BlockingQueue&&) = delete;

    public:
        explicit BlockingQueue(std::size_t max_size = 0) :
            m_max_size(max_size)
        {
        }

        /// @brief 向队列写入一个元素。
        /// @retval true  写入成功。
        /// @retval false 队列已经关闭，元素未写入。
        [[nodiscard]] bool Push(T value)
        {
            std::unique_lock lock(this->m_mtx);
            this->m_cv_can_push.wait(lock, [this] { return this->m_is_closed || this->m_max_size == 0 || this->m_queue.size() < this->m_max_size; });

            if (this->m_is_closed) {
                return false;
            }

            this->m_queue.emplace(std::move(value));
            lock.unlock();
            this->m_cv_can_pop.notify_one();
            return true;
        }

        /// @brief 从队列取出一个元素。
        /// @return 队列有数据时返回元素；队列关闭且已取尽时返回 std::nullopt。
        [[nodiscard]] std::optional<T> Pop()
        {
            std::unique_lock lock(this->m_mtx);
            this->m_cv_can_pop.wait(lock, [this] { return this->m_is_closed || !this->m_queue.empty(); });

            if (this->m_queue.empty()) {
                return std::nullopt;
            }

            T value = std::move(this->m_queue.front());
            this->m_queue.pop();
            lock.unlock();
            this->m_cv_can_push.notify_one();
            return value;
        }

        /// @brief 关闭队列并唤醒所有等待线程。
        /// @note Close() 不会清空队列，消费者仍可继续取出关闭前已经写入的数据。
        void Close()
        {
            {
                std::lock_guard lock(this->m_mtx);
                this->m_is_closed = true;
            }
            this->m_cv_can_push.notify_all();
            this->m_cv_can_pop.notify_all();
        }

        /// @brief 查询队列是否已经关闭。
        [[nodiscard]] bool IsClosed() const
        {
            std::lock_guard lock(this->m_mtx);
            return this->m_is_closed;
        }

        /// @brief 查询当前队列元素数量。
        [[nodiscard]] std::size_t Size() const
        {
            std::lock_guard lock(this->m_mtx);
            return this->m_queue.size();
        }

        /// @brief 查询队列容量；0 表示无界队列。
        [[nodiscard]] std::size_t Capacity() const noexcept { return this->m_max_size; }

    private:
        mutable std::mutex m_mtx;
        std::condition_variable m_cv_can_push;
        std::condition_variable m_cv_can_pop;
        std::queue<T> m_queue;
        std::size_t m_max_size = 0;
        bool m_is_closed = false;
    };
} // namespace utils
