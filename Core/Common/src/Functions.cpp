#include "Functions.h"

#include <algorithm>
#include <iterator>

namespace {
    constexpr unsigned char ToAsciiLower(unsigned char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
    }
} // namespace

std::size_t FindCaseInsensitive(std::string_view main_str, std::string_view sub_str) noexcept
{
    if (sub_str.empty()) {
        return 0;
    }

    const auto res =
        std::ranges::search(main_str, sub_str, [](unsigned char lhs, unsigned char rhs) { return ToAsciiLower(lhs) == ToAsciiLower(rhs); });
    if (res.begin() == main_str.end()) {
        return std::string_view::npos;
    }

    return static_cast<std::size_t>(std::ranges::distance(main_str.begin(), res.begin()));
}

bool IEquals(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    return std::ranges::equal(lhs, rhs, [](unsigned char lhs_char, unsigned char rhs_char) {
        return ToAsciiLower(lhs_char) == ToAsciiLower(rhs_char);
    });
}

std::string_view TrimSpaces(const std::string_view str) noexcept
{
    constexpr std::string_view whitespace_chars = " \t\n\r\f\v";
    const std::size_t first = str.find_first_not_of(whitespace_chars);
    if (first == std::string_view::npos) {
        return { };
    }

    const std::size_t last = str.find_last_not_of(whitespace_chars);
    return str.substr(first, last - first + 1);
}

std::string_view StripEdgeChar(std::string_view str, char c) noexcept
{
    if (str.empty()) {
        return str;
    }
    if (str.front() == c) {
        str.remove_prefix(1);
    }
    if (!str.empty() && str.back() == c) {
        str.remove_suffix(1);
    }
    return str;
}
