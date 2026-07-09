#pragma once
#include <format>
#include <vector>
#include <array>
#include <cmath>

namespace _container_ {
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

    template<class _Ty>
    concept Clearable = std::default_initializable<_Ty> && std::movable<_Ty>;
} // namespace _container_

namespace utils {
    template<class ValueType>
    concept VectorType = _container_::is_std_vector_v<ValueType>;

    template<class ValueType>
    concept ArrayType = _container_::is_std_array_v<ValueType>;

    template<std::floating_point _Ty>
    constexpr bool almost_equal(_Ty lhs, _Ty rhs) noexcept
    {
        constexpr auto get_tolerance = []() -> std::pair<_Ty, _Ty> {
            if constexpr (std::same_as<_Ty, float>) {
                return { 1e-5f, 1e-7f };
            }
            else if constexpr (std::same_as<_Ty, double>) {
                return { 1e-9, 1e-12 };
            }
            else { // long double or custom
                return { 1e-12L, 1e-15L };
            }
        };
        const auto [rel_tol, abs_tol] = get_tolerance();

        const _Ty diff = std::fabs(lhs - rhs);
        const _Ty scale = std::max(std::fabs(lhs), std::fabs(rhs));

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
    requires std::constructible_from<ValueType, std::remove_cvref_t<std::ranges::range_reference_t<Range>>>
    constexpr void AppendVector(std::vector<ValueType>& target, Range&& source)
    {
        target.insert(target.end(), std::ranges::begin(source), std::ranges::end(source));
    }

    template<class ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, const std::vector<ValueType>& source)
    {
        target.insert(target.end(), source.begin(), source.end());
    }

    template<class ValueType>
    constexpr void AppendVector(std::vector<ValueType>& target, std::vector<ValueType>&& source)
    {
        target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
    }

    template<class ValueType, class _Ty>
    constexpr void AppendVector(std::vector<ValueType>& target, std::size_t count, _Ty&& value)
    {
        if constexpr (std::is_lvalue_reference_v<_Ty>) {
            target.insert(target.end(), count, value);
        }
        else {
            for (std::size_t i = 0; i < count; ++i) {
                target.emplace_back(value);
            }
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

    template<_container_::Clearable... Args>
    constexpr void DeepClear(Args&... vecs)
    {
        ((vecs = Args()), ...);
    }

    template<class... Args>
    constexpr void VectorShrink(Args&... vecs)
    {
        auto lambda = [](auto& v) {
            if constexpr (requires { v.shrink_to_fit(); }) {
                v.shrink_to_fit();
            }
        };

        (lambda(vecs), ...);
    }
} // namespace utils

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y)      CONCAT_IMPL(x, y)
