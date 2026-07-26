#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

namespace utils {
    template<typename T>
    using PrefixSumType_t = std::conditional_t<std::floating_point<T>,
                                               std::conditional_t<(sizeof(T) < sizeof(double)), double, T>,
                                               std::conditional_t<std::is_unsigned_v<T>, std::uint64_t, std::int64_t>>;

    template<typename T>
    using SafeSumType_t [[deprecated("use PrefixSumType_t")]] = PrefixSumType_t<T>;

    // 单线程增量前缀和缓存。
    //
    // 线程安全：非线程安全。query() 虽为 const，但会修改内部缓存
    //   (last_idx/last_sum/has_cache)，多个线程对同一实例并发 query 是数据竞争。
    //
    // 缓存约束：假设源数组“只读 / 只追加”。增量路径基于上次缓存的累加值前后滑动，
    //   若修改了已缓存范围 [0, last_idx] 内的任何元素，必须调用 invalidate()，
    //   否则后续 query 会基于过期的 last_sum 返回错误结果。
    template<typename T, typename SumType = PrefixSumType_t<T>>
    requires(std::integral<T> || std::floating_point<T>) && (std::integral<SumType> || std::floating_point<SumType>)
    class SmartPrefixSum {
        SmartPrefixSum(std::vector<T>&&) = delete;
        SmartPrefixSum(const std::vector<T>&&) = delete;

    public:
        explicit SmartPrefixSum(const std::vector<T>& array) :
            m_vec_data(array)
        {
        }

        // 源数据被修改后调用，丢弃缓存，下次 query 全量重算
        void invalidate() noexcept
        {
            this->m_has_cache = false;
            this->m_last_idx = 0;
            this->m_last_sum = SumType { };
        }

        [[nodiscard]] SumType query(std::size_t index) const
        {
            if (index >= this->m_vec_data.size()) {
                return SumType { };
            }

            if (!this->m_has_cache || index < 1000) {
                return reset_and_calc(index);
            }

            if (index >= this->m_last_idx) {
                const std::size_t diff = index - this->m_last_idx;
                if (diff < 5'000'000) {
                    std::span<const T> view { this->m_vec_data.data() + this->m_last_idx + 1, diff };
                    this->m_last_sum = std::accumulate(view.begin(), view.end(), this->m_last_sum);
                    this->m_last_idx = index;
                    return this->m_last_sum;
                }
            }
            else {
                const std::size_t diff = this->m_last_idx - index;
                if (diff < 5'000'000) {
                    std::span<const T> view { this->m_vec_data.data() + index + 1, diff };
                    this->m_last_sum = this->m_last_sum - std::accumulate(view.begin(), view.end(), SumType { });
                    this->m_last_idx = index;
                    return this->m_last_sum;
                }
            }

            return reset_and_calc(index);
        }

    private:
        SumType reset_and_calc(std::size_t index) const
        {
            std::span<const T> view { this->m_vec_data.data(), index + 1 };
            if (view.size() < 100000) {
                this->m_last_sum = std::accumulate(view.begin(), view.end(), SumType { });
            }
            else {
                this->m_last_sum = std::reduce(std::execution::par, view.begin(), view.end(), SumType { });
            }
            this->m_last_idx = index;
            this->m_has_cache = true;
            return this->m_last_sum;
        }

        const std::vector<T>& m_vec_data;

        mutable std::size_t m_last_idx = 0;
        mutable SumType m_last_sum = 0;
        mutable bool m_has_cache = false;
    };
} // namespace utils
