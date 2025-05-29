#pragma once
#include <format>
#include <vector>
#include <ranges>
#include <concepts>
#include <iostream>

#include <type_traits>
#include <stdexcept>
#include <cmath>
#include <string>

constexpr std::string_view type_name_mapping(const char* raw_name)
{
    if (std::string_view(raw_name) == "i") {
        return "int";
    }
    if (std::string_view(raw_name) == "j") {
        return "unsigned int";
    }
    if (std::string_view(raw_name) == "l") {
        return "long";
    }
    if (std::string_view(raw_name) == "m") {
        return "unsigned long";
    }
    if (std::string_view(raw_name) == "f") {
        return "float";
    }
    if (std::string_view(raw_name) == "d") {
        return "double";
    }
    return raw_name;
}

template<typename OriginalType, typename TargetType>
TargetType ChangeType(const OriginalType& value)
{
    static_assert(std::is_arithmetic_v<OriginalType>, "OriginalType must be arithmetic");
    static_assert(std::is_arithmetic_v<TargetType>, "TargetType must be arithmetic");

    constexpr TargetType target_max = std::numeric_limits<TargetType>::max();
    constexpr TargetType target_min = std::numeric_limits<TargetType>::lowest();

    if constexpr (std::is_integral_v<TargetType> && std::is_floating_point_v<OriginalType>) {
        // 处理NaN
        if (std::isnan(value)) {
            throw std::runtime_error(std::format("Cannot convert NaN to {}", type_name_mapping(typeid(TargetType).name())));
        }

        if (value > static_cast<OriginalType>(target_max)) {
            return target_max;
        }

        if (value < static_cast<OriginalType>(target_min)) {
            return target_min;
        }

        return static_cast<TargetType>(value);
    }
    else {
        if (value >= target_min && value <= target_max) {
            return static_cast<TargetType>(value);
        }

        // 超界处理
        return (value > static_cast<OriginalType>(0)) ? target_max : target_min;
    }
}

template<typename TargetType, typename OriginalType>
constexpr TargetType SafeCast(OriginalType value) noexcept
{
    static_assert(std::is_arithmetic_v<OriginalType>, "OriginalType must be arithmetic");
    static_assert(std::is_arithmetic_v<TargetType>, "TargetType must be arithmetic");

    if constexpr (std::is_constant_evaluated()) {
        static_assert(
            std::numeric_limits<OriginalType>::lowest() >= std::numeric_limits<TargetType>::lowest() &&
                std::numeric_limits<OriginalType>::max() <= std::numeric_limits<TargetType>::max(),
            "Unsafe cast in compile-time context");
        return static_cast<TargetType>(value);
    }
    else {
        return ChangeType<OriginalType, TargetType>(value);
    }
}

template<typename OriginalType, typename TgargetType>
constexpr inline std::vector<TgargetType> ShrinkVector(const std::vector<OriginalType>& data)
{
    std::vector<TgargetType> result;
    result.reserve(data.size());
    for (const auto& item : data) {
        try {
            result.emplace_back(ChangeType<OriginalType, TgargetType>(item));
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }
    return result;
}

template<class _Ty>
requires std::copy_constructible<_Ty>
constexpr inline void AppendVector(std::vector<_Ty>& target, const std::vector<_Ty>& source)
{
    target.insert(target.end(), source.begin(), source.end());
}

template<class _Ty>
requires std::movable<_Ty>
constexpr inline void AppendVector(std::vector<_Ty>& target, std::vector<_Ty>&& source)
{
    target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}

template<class _Ty, class Range>
requires std::ranges::input_range<Range> && std::constructible_from<_Ty, std::ranges::range_reference_t<Range>>
constexpr inline void AppendVector(std::vector<_Ty>& target, Range&& source)
{
    target.insert(target.end(), std::ranges::begin(source), std::ranges::begin(source));
}
