#pragma once
#include <format>
#include <vector>

namespace _contatiner_ {
    template<class ValueType>
    struct is_std_contatiner : std::false_type
    {
    };

    template<class ValueType, class Alloc>
    struct is_std_contatiner<std::vector<ValueType, Alloc>> : std::true_type
    {
    };
}
template<class ValueType>
concept VectorType = _contatiner_::is_std_contatiner<std::remove_cvref_t<ValueType>>::value;

namespace _hidden_ {
    /// 溢出处理策略
    enum class OverflowPolicy : int
    {
        Exception,  // 抛异常
        Clip,       // 裁剪至上下界
        DefaultZero // 超出时返回零
    };

    /// 运行时安全转换
    template<class OriginalType, class TargetType, OverflowPolicy Policy = OverflowPolicy::Exception>
    requires std::is_arithmetic_v<OriginalType> && std::is_arithmetic_v<TargetType>
    inline TargetType SafeCastRuntime(const OriginalType& value)
    {
        constexpr TargetType target_max = std::numeric_limits<TargetType>::max();
        constexpr TargetType target_min = std::numeric_limits<TargetType>::lowest();
        constexpr OriginalType omin = static_cast<OriginalType>(target_min);
        constexpr OriginalType omax = static_cast<OriginalType>(target_max);

        auto handle_overflow = [&](const char* reason, TargetType fallback) -> TargetType {
            if constexpr (Policy == OverflowPolicy::Exception) {
                throw std::runtime_error(
                    std::format("SafeCastRuntime error ({}): cannot convert {} to {}", reason, value, typeid(TargetType).name()));
            }
            else if constexpr (Policy == OverflowPolicy::Clip) {
                return fallback;
            }
            else {
                return TargetType(0);
            }
        };

        // 浮点 -> 整型，需要特别处理
        if constexpr (std::is_floating_point_v<OriginalType> && std::is_integral_v<TargetType>) {
            if (std::isnan(value)) {
                return handle_overflow("NaN", 0);
            }
            if (!std::isfinite(value)) {
                return handle_overflow("Infinite", 0);
            }
            if (value > omax) {
                return handle_overflow("Overflow", target_max);
            }
            if (value < omin) {
                return handle_overflow("Underflow", target_min);
            }
            return static_cast<TargetType>(value);
        }
        // 其他类型转换（包括整型<->整型，整型->浮点，浮点->浮点）
        else {
            if (value > omax) {
                return handle_overflow("Overflow", target_max);
            }
            if (value < omin) {
                return handle_overflow("Underflow", target_min);
            }
            return static_cast<TargetType>(value);
        }
    }

    /// 编译期安全转换
    template<class OriginalType, class TargetType>
    requires std::is_arithmetic_v<OriginalType> && std::is_arithmetic_v<TargetType>
    constexpr inline TargetType SafeCastConstexpr(const OriginalType& value)
    {
        if (value < static_cast<OriginalType>(std::numeric_limits<TargetType>::lowest()) ||
            value > static_cast<OriginalType>(std::numeric_limits<TargetType>::max())) {
            throw std::runtime_error("SafeCastConstexpr: value out of range");
        }
        return static_cast<TargetType>(value);
    }
}

/// 智能选择 编译/运行 的统一入口
template<class TargetType, class OriginalType, _hidden_::OverflowPolicy Policy = _hidden_::OverflowPolicy::Clip>
requires std::is_arithmetic_v<OriginalType> && std::is_arithmetic_v<TargetType>
constexpr inline TargetType SafeCast(OriginalType value)
{
    if constexpr (std::is_constant_evaluated()) {
        return _hidden_::SafeCastConstexpr<OriginalType, TargetType>(value);
    }
    else {
        return _hidden_::SafeCastRuntime<OriginalType, TargetType, Policy>(value);
    }
}

template<std::floating_point _Ty>
constexpr bool almost_equal(_Ty a, _Ty b)
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

    const _Ty diff = std::fabs(a - b);
    const _Ty scale = std::max(std::fabs(a), std::fabs(b));

    return diff <= std::max(rel_tol * scale, abs_tol);
}

template<class TgargetType, class OriginalType>
inline std::vector<TgargetType> ShrinkVector(const std::vector<OriginalType>& data)
{
    std::vector<TgargetType> result;
    result.reserve(data.size());
    for (const auto& item : data) {
        result.emplace_back(SafeCast<TgargetType, OriginalType>(item));
    }
    return result;
}

template<class ValueType>
requires std::copy_constructible<ValueType>
constexpr inline void AppendVector(std::vector<ValueType>& target, const std::vector<ValueType>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

template<class ValueType>
requires std::movable<ValueType>
constexpr inline void AppendVector(std::vector<ValueType>& target, std::vector<ValueType>&& source)
{
    target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}

template<class ValueType, std::ranges::input_range Range>
requires std::constructible_from<ValueType, std::remove_cvref_t<std::ranges::range_reference_t<Range>>>
constexpr inline void AppendVector(std::vector<ValueType>& target, Range&& source)
{
    target.insert(target.end(), std::ranges::begin(source), std::ranges::end(source));
}

template<class ValueType>
constexpr inline void AppendVector(std::vector<ValueType>& target, std::initializer_list<ValueType> source)
{
    target.insert(target.end(), source.begin(), source.end());
}

template<class ValueType, class _Ty>
constexpr void AppendVector(std::vector<ValueType>& target, std::size_t count, _Ty&& value)
{
    if constexpr (std::is_lvalue_reference_v<_Ty>) {
    target.insert(target.end(), count, value);
}
    else {
        for (std::size_t i = 0; i < count; ++i) {
        target.emplace_back(std::move(value));
    }
}
}

template<class ValueType = int>
requires requires(ValueType value, std::size_t i) {
    { value + value } -> std::convertible_to<ValueType>;
    { static_cast<ValueType>(i) * value } -> std::convertible_to<ValueType>;
}
constexpr inline std::vector<ValueType> CreateVector(std::size_t count, ValueType start = 0, ValueType step = 1)
{
    std::vector<ValueType> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.emplace_back(start + static_cast<ValueType>(i) * step);
    }
    return result;
}

#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y)      CONCAT_IMPL(x, y)
