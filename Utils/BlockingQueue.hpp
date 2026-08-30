#pragma once
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
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
    public:
        BlockingQueue(const BlockingQueue&) = delete;
        BlockingQueue& operator=(const BlockingQueue&) = delete;
        BlockingQueue(BlockingQueue&&) = delete;
        BlockingQueue& operator=(BlockingQueue&&) = delete;

        explicit BlockingQueue(std::size_t max_size = 0) :
            m_max_size(max_size)
        {
        }

        /// @brief 在队列中原地构造一个元素。
        /// @retval true  写入成功。
        /// @retval false 队列已经关闭，元素未构造。
        template<class... Args>
        requires std::constructible_from<T, Args...>
        [[nodiscard]] bool Emplace(Args&&... args)
        {
            std::unique_lock lock(this->m_mtx);
            this->m_cv_can_push.wait(lock, [this] { return this->m_is_closed || this->m_max_size == 0 || this->m_queue.size() < this->m_max_size; });

            if (this->m_is_closed) {
                return false;
            }

            this->m_queue.emplace(std::forward<Args>(args)...);
            lock.unlock();
            this->m_cv_can_pop.notify_one();
            return true;
        }

        /// @brief 可取消地在队列中原地构造一个元素。
        /// @retval false 队列已经关闭或停止已被请求，元素未构造。
        template<class... Args>
        requires std::constructible_from<T, Args...>
        [[nodiscard]] bool Emplace(const std::stop_token stop_token, Args&&... args)
        {
            std::unique_lock lock(this->m_mtx);
            const bool ready = this->m_cv_can_push.wait(lock, stop_token, [this] {
                return this->m_is_closed || this->m_max_size == 0 || this->m_queue.size() < this->m_max_size;
            });

            if (!ready || stop_token.stop_requested() || this->m_is_closed) {
                return false;
            }

            this->m_queue.emplace(std::forward<Args>(args)...);
            lock.unlock();
            this->m_cv_can_pop.notify_one();
            return true;
        }

        [[nodiscard]] bool Push(T&& value) { return this->Emplace(std::move(value)); }

        [[nodiscard]] bool Push(const T& value)
        requires std::copy_constructible<T>
        {
            return this->Emplace(value);
        }

        [[nodiscard]] bool Push(const std::stop_token stop_token, T&& value) { return this->Emplace(stop_token, std::move(value)); }

        [[nodiscard]] bool Push(const std::stop_token stop_token, const T& value)
        requires std::copy_constructible<T>
        {
            return this->Emplace(stop_token, value);
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

        /// @brief 可取消地从队列取出一个元素。
        /// @return 队列有数据时返回元素；队列关闭、取尽或停止已被请求时返回 std::nullopt。
        [[nodiscard]] std::optional<T> Pop(const std::stop_token stop_token)
        {
            std::unique_lock lock(this->m_mtx);
            const bool ready = this->m_cv_can_pop.wait(lock, stop_token, [this] { return this->m_is_closed || !this->m_queue.empty(); });

            if (!ready || stop_token.stop_requested() || this->m_queue.empty()) {
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
        std::condition_variable_any m_cv_can_push;
        std::condition_variable_any m_cv_can_pop;
        std::queue<T> m_queue;
        std::size_t m_max_size = 0;
        bool m_is_closed = false;
    };
} // namespace utils
