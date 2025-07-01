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
    enum class OverflowPolicy
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

        // 浮点 → 整型，需要特殊处理
        if constexpr (std::is_integral_v<TargetType> && std::is_floating_point_v<OriginalType>) {
            if (std::isnan(value)) {
                if constexpr (Policy == OverflowPolicy::Exception) {
                    throw std::runtime_error(std::format("Cannot convert NaN value to {}", typeid(TargetType).name()));
                }
                else {
                    return TargetType(0);
                }
            }
            if (!std::isfinite(value)) {
                if constexpr (Policy == OverflowPolicy::Exception) {
                    throw std::runtime_error(std::format("Cannot convert infinite value to {}", typeid(TargetType).name()));
                }
                else {
                    return TargetType(0);
                }
            }

            constexpr OriginalType omax = static_cast<OriginalType>(target_max);
            constexpr OriginalType omin = static_cast<OriginalType>(target_min);
            if (value > omax) {
                if constexpr (Policy == OverflowPolicy::Exception) {
                    throw std::runtime_error(std::format("Overflow: {} > max({})", value, typeid(TargetType).name()));
                }
                else if constexpr (Policy == OverflowPolicy::Clip) {
                    return target_max;
                }
                else {
                    return TargetType(0);
                }
            }
            if (value < omin) {
                if constexpr (Policy == OverflowPolicy::Exception) {
                    throw std::runtime_error(std::format("Underflow: {} < min({})", value, typeid(TargetType).name()));
                }
                else if constexpr (Policy == OverflowPolicy::Clip) {
                    return target_min;
                }
                else {
                    return TargetType(0);
                }
            }
            return static_cast<TargetType>(value);
        }
        else {
            constexpr OriginalType omin = static_cast<OriginalType>(target_min);
            constexpr OriginalType omax = static_cast<OriginalType>(target_max);
            if (value >= omin && value <= omax) {
                return static_cast<TargetType>(value);
            }

            if constexpr (Policy == OverflowPolicy::Exception) {
                throw std::runtime_error(std::format("Value {} out of range for {}", value, typeid(TargetType).name()));
            }
            else if constexpr (Policy == OverflowPolicy::Clip) {
                return (value > OriginalType(0)) ? target_max : target_min;
            }
            else {
                return TargetType(0);
            }
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

template<class OriginalType, class TgargetType>
inline std::vector<TgargetType> ShrinkVector(const std::vector<OriginalType>& data)
{
    std::vector<TgargetType> result;
    result.reserve(data.size());
    for (const auto& item : data) {
        result.emplace_back(SafeCast<OriginalType, TgargetType>(item), _hidden_::OverflowPolicy::Clip);
    }
    return result;
}

/// 将 source 中的元素复制追加到 target。
/// \tparam ValueType 元素类型
/// \param target 目标 vector
/// \param source 要复制的 vector
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

template<class ValueType = int>
requires requires(ValueType value, std::size_t i) {
    { value + value } -> std::convertible_to<ValueType>;
    { static_cast<ValueType>(i) * value } -> std::convertible_to<ValueType>;
}
constexpr auto CreateVector(std::size_t count, ValueType start = 0, ValueType step = 1)
{
    std::vector<ValueType> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        result.emplace_back(start + static_cast<ValueType>(i) * step);
    }
    return result;
}
