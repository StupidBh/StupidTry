#pragma once
#include <concepts>
#include <cstdint>
#include <execution>
#include <numeric>
#include <span>
#include <type_traits>
#include <vector>

namespace stupid {
    template<typename T>
    using SafeSumType_t = std::conditional_t<std::is_floating_point_v<T>, double, std::conditional_t<std::is_unsigned_v<T>, uint64_t, int64_t>>;

    // 单线程增量前缀和缓存。
    //
    // 线程安全：非线程安全。query() 虽为 const，但会修改内部缓存
    //   (last_idx/last_sum/has_cache)，多个线程对同一实例并发 query 是数据竞争。
    //
    // 缓存约束：假设源数组“只读 / 只追加”。增量路径基于上次缓存的累加值前后滑动，
    //   若修改了已缓存范围 [0, last_idx] 内的任何元素，必须调用 invalidate()，
    //   否则后续 query 会基于过期的 last_sum 返回错误结果。
    template<typename T, typename SumType = SafeSumType_t<T>>
    requires(std::integral<T> || std::floating_point<T>) && (std::integral<SumType> || std::floating_point<SumType>)
    class SmartPrefixSum {
        const std::vector<T>& A;

        mutable size_t last_idx = 0;
        mutable SumType last_sum = 0;
        mutable bool has_cache = false;

    public:
        explicit SmartPrefixSum(const std::vector<T>& array) :
            A(array)
        {
        }

        // 源数据被修改后调用，丢弃缓存，下次 query 全量重算
        void invalidate() noexcept
        {
            has_cache = false;
            last_idx = 0;
            last_sum = SumType { };
        }

        SumType query(size_t index) const
        {
            if (index >= A.size()) {
                return SumType { };
            }

            if (!has_cache || index < 1000) {
                return reset_and_calc(index);
            }

            if (index >= last_idx) {
                size_t diff = index - last_idx;
                if (diff < 5'000'000) {
                    std::span<const T> view { A.data() + last_idx + 1, diff };
                    last_sum = std::accumulate(view.begin(), view.end(), last_sum);
                    last_idx = index;
                    return last_sum;
                }
            }
            else {
                size_t diff = last_idx - index;
                if (diff < 5'000'000) {
                    std::span<const T> view { A.data() + index + 1, diff };
                    last_sum = last_sum - std::accumulate(view.begin(), view.end(), SumType { });
                    last_idx = index;
                    return last_sum;
                }
            }

            return reset_and_calc(index);
        }

    private:
        SumType reset_and_calc(size_t index) const
        {
            std::span<const T> view { A.data(), index + 1 };
            if (view.size() < 100000) {
                last_sum = std::accumulate(view.begin(), view.end(), SumType { });
            }
            else {
                last_sum = std::reduce(std::execution::par, view.begin(), view.end(), SumType { });
            }
            last_idx = index;
            has_cache = true;
            return last_sum;
        }
    };
} // namespace stupid
