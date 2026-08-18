#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace utils::detail {
    template<class ValueType>
    struct is_std_vector : std::false_type
    {
    };

    template<class ValueType, class Alloc>
    struct is_std_vector<std::vector<ValueType, Alloc>> : std::true_type
    {
    };

    template<class T>
    inline constexpr bool is_std_vector_v = is_std_vector<std::remove_cvref_t<T>>::value;

    template<class ValueType>
    struct is_std_array : std::false_type
    {
    };

    template<class ValueType, std::size_t N>
    struct is_std_array<std::array<ValueType, N>> : std::true_type
    {
    };

    template<class T>
    inline constexpr bool is_std_array_v = is_std_array<std::remove_cvref_t<T>>::value;

    template<class Ty>
    concept Clearable = std::default_initializable<Ty> && std::movable<Ty>;

    template<class Ty>
    concept Shrinkable = requires(Ty& value) { value.shrink_to_fit(); };

    template<class ValueType>
    constexpr void ReserveAdditional(std::vector<ValueType>& target, const std::size_t count)
    {
        if (count > target.max_size() - target.size()) {
            throw std::length_error("AppendVector exceeds vector max_size");
        }
        target.reserve(target.size() + count);
    }

    template<std::move_constructible ValueType>
    constexpr void AppendBuffered(std::vector<ValueType>& target, std::vector<ValueType>& buffered)
    {
        ReserveAdditional(target, buffered.size());
        for (auto& value : buffered) {
            target.emplace_back(std::move(value));
        }
    }
} // namespace utils::detail

namespace utils {
    template<class ValueType>
    concept VectorType = detail::is_std_vector_v<ValueType>;

    template<class ValueType>
    concept ArrayType = detail::is_std_array_v<ValueType>;

    template<std::floating_point Ty>
    constexpr bool almost_equal(Ty lhs, Ty rhs) noexcept
    {
        if (lhs == rhs) {
            return true;
        }
        if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
            return false;
        }

        constexpr auto get_tolerance = []() -> std::pair<Ty, Ty> {
            if constexpr (std::same_as<Ty, float>) {
                return { 1e-5f, 1e-7f };
            }
            else if constexpr (std::same_as<Ty, double>) {
                return { 1e-9, 1e-12 };
            }
            else { // long double or custom
                return { 1e-12L, 1e-15L };
            }
        };
        const auto [rel_tol, abs_tol] = get_tolerance();

        const Ty diff = std::fabs(lhs - rhs);
        const Ty scale = std::max(std::fabs(lhs), std::fabs(rhs));

        return diff <= std::max(rel_tol * scale, abs_tol);
    }

    template<class TargetType, VectorType OriginalType>
    constexpr auto ShrinkVector(const OriginalType& source)
    {
        using SourceType = typename OriginalType::value_type;
        if constexpr (std::same_as<TargetType, SourceType>) {
            return source;
        }

        std::vector<TargetType> result;
        result.reserve(source.size());
        for (const auto& value : source) {
            result.emplace_back(static_cast<TargetType>(value));
        }
        return result;
    }

    template<class ValueType, std::ranges::input_range Range>
    requires(!VectorType<Range>) && std::constructible_from<ValueType, std::ranges::range_reference_t<Range>> && std::move_constructible<ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, Range&& source)
    {
        std::vector<ValueType> buffered;
        if constexpr (std::ranges::sized_range<Range>) {
            buffered.reserve(static_cast<std::size_t>(std::ranges::size(source)));
        }
        for (auto&& value : source) {
            buffered.emplace_back(std::forward<decltype(value)>(value));
        }
        detail::AppendBuffered(target, buffered);
    }

    template<class ValueType, class SourceType, class Allocator>
    requires std::constructible_from<ValueType, const SourceType&> && std::move_constructible<ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, const std::vector<SourceType, Allocator>& source)
    {
        std::vector<ValueType> buffered;
        buffered.reserve(source.size());
        for (const auto& value : source) {
            buffered.emplace_back(value);
        }
        detail::AppendBuffered(target, buffered);
    }

    template<class ValueType, class SourceType, class Allocator>
    requires std::constructible_from<ValueType, SourceType&&> && std::move_constructible<ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, std::vector<SourceType, Allocator>&& source)
    {
        if constexpr (std::same_as<std::vector<ValueType>, std::vector<SourceType, Allocator>>) {
            if (std::addressof(target) == std::addressof(source)) {
                throw std::invalid_argument("AppendVector cannot move a vector into itself");
            }
        }

        std::vector<ValueType> buffered;
        buffered.reserve(source.size());
        for (auto& value : source) {
            buffered.emplace_back(std::move(value));
        }
        detail::AppendBuffered(target, buffered);
    }

    template<class ValueType, class SourceType>
    requires std::constructible_from<ValueType, SourceType&&> && std::copy_constructible<ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, std::size_t count, SourceType&& value)
    {
        if (count == 0) {
            return;
        }
        const ValueType stable_value(std::forward<SourceType>(value));
        detail::ReserveAdditional(target, count);
        for (std::size_t index = 0; index < count; ++index) {
            target.emplace_back(stable_value);
        }
    }

    template<class ValueType = int>
    requires requires(ValueType value, std::size_t i) {
        { value + value } -> std::convertible_to<ValueType>;
        { static_cast<ValueType>(i) * value } -> std::convertible_to<ValueType>;
    }
    constexpr std::vector<ValueType> CreateVector(std::size_t count, ValueType start = 0, ValueType step = 1)
    {
        std::vector<ValueType> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            result.emplace_back(start + static_cast<ValueType>(i) * step);
        }
        return result;
    }

    template<detail::Clearable... Args>
    constexpr void DeepClear(Args&... vecs)
    {
        ((vecs = Args()), ...);
    }

    template<detail::Shrinkable... Args>
    requires(sizeof...(Args) > 0)
    constexpr void VectorShrink(Args&... vecs)
    {
        (vecs.shrink_to_fit(), ...);
    }
} // namespace utils
