#pragma once
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <execution>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
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

    enum class PrefixSumExecution
    {
        adaptive,
        sequenced,
        parallel
    };

    struct PrefixSumPolicy
    {
        std::size_t direct_query_threshold = 1000;
        std::size_t incremental_distance = 5'000'000;
        std::size_t parallel_threshold = 100000;
        PrefixSumExecution execution = PrefixSumExecution::adaptive;
    };

    template<class Value, class Sum>
    concept PrefixSummable =
        (std::integral<Value> || std::floating_point<Value>) && (std::integral<Sum> || std::floating_point<Sum>) && requires(Sum sum, Value value) {
            { sum + value } -> std::convertible_to<Sum>;
            { sum - value } -> std::convertible_to<Sum>;
        };

    // 单线程增量前缀和缓存。
    //
    // 线程安全：非线程安全。query() 虽为 const，但会修改内部缓存
    //   (last_idx/last_sum/has_cache)，多个线程对同一实例并发 query 是数据竞争。
    //
    // 缓存约束：假设 vector 源“只读 / 只追加”，其他连续范围的长度固定。增量路径基于上次缓存的累加值前后滑动，
    //   若修改了已缓存范围 [0, last_idx] 内的任何元素，必须调用 invalidate()，
    //   否则后续 query 会基于过期的 last_sum 返回错误结果。
    template<typename T, typename SumType = PrefixSumType_t<T>>
    requires PrefixSummable<T, SumType>
    class SmartPrefixSum {
    public:
        SmartPrefixSum(std::vector<T>&&, PrefixSumPolicy = { }) = delete;
        SmartPrefixSum(const std::vector<T>&&, PrefixSumPolicy = { }) = delete;

        explicit SmartPrefixSum(const std::vector<T>& array, PrefixSumPolicy policy = { }) :
            m_vector_data(std::addressof(array)),
            m_policy(policy)
        {
        }

        template<std::ranges::contiguous_range Range>
        requires std::ranges::sized_range<Range> && std::ranges::borrowed_range<Range> && std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, T> &&
                     (!std::same_as<std::remove_cvref_t<Range>, std::vector<T>>)
        explicit SmartPrefixSum(Range&& range, PrefixSumPolicy policy = { }) :
            m_static_data(std::ranges::data(range), std::ranges::size(range)),
            m_policy(policy)
        {
        }

        // 源数据被修改后调用，丢弃缓存，下次 query 全量重算
        void invalidate() const noexcept
        {
            this->m_has_cache = false;
            this->m_last_idx = 0;
            this->m_last_sum = SumType { };
        }

        [[nodiscard]] SumType query(std::size_t index) const { return this->try_query(index).value_or(SumType { }); }

        [[nodiscard]] std::optional<SumType> try_query(std::size_t index) const
        {
            const std::span<const T> data = this->current_data();
            if (index >= data.size()) {
                return std::nullopt;
            }

            if (this->m_has_cache && this->m_last_idx >= data.size()) {
                this->invalidate();
            }

            if (!this->m_has_cache || index < this->m_policy.direct_query_threshold) {
                return this->reset_and_calc(data, index);
            }

            if (index >= this->m_last_idx) {
                const std::size_t diff = index - this->m_last_idx;
                if (diff < this->m_policy.incremental_distance) {
                    const std::span<const T> view = data.subspan(this->m_last_idx + 1, diff);
                    this->m_last_sum = std::ranges::fold_left(view, this->m_last_sum, std::plus { });
                    this->m_last_idx = index;
                    return this->m_last_sum;
                }
            }
            else {
                const std::size_t diff = this->m_last_idx - index;
                if (diff < this->m_policy.incremental_distance) {
                    const std::span<const T> view = data.subspan(index + 1, diff);
                    this->m_last_sum = this->m_last_sum - std::ranges::fold_left(view, SumType { }, std::plus { });
                    this->m_last_idx = index;
                    return this->m_last_sum;
                }
            }

            return this->reset_and_calc(data, index);
        }

    private:
        [[nodiscard]] std::span<const T> current_data() const noexcept
        {
            if (this->m_vector_data != nullptr) {
                return std::span<const T>(*this->m_vector_data);
            }
            return this->m_static_data;
        }

        SumType reset_and_calc(const std::span<const T> data, std::size_t index) const
        {
            const std::span<const T> view = data.first(index + 1);
            const bool use_parallel = this->m_policy.execution == PrefixSumExecution::parallel ||
                                      (this->m_policy.execution == PrefixSumExecution::adaptive && view.size() >= this->m_policy.parallel_threshold);
            if (use_parallel) {
                this->m_last_sum = std::reduce(std::execution::par, view.begin(), view.end(), SumType { });
            }
            else {
                this->m_last_sum = std::ranges::fold_left(view, SumType { }, std::plus { });
            }
            this->m_last_idx = index;
            this->m_has_cache = true;
            return this->m_last_sum;
        }

        const std::vector<T>* m_vector_data = nullptr;
        std::span<const T> m_static_data;
        PrefixSumPolicy m_policy;

        mutable std::size_t m_last_idx = 0;
        mutable SumType m_last_sum { };
        mutable bool m_has_cache = false;
    };
} // namespace utils
