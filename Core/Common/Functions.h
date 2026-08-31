#pragma once
#include <string_view>

/// Byte-wise comparisons with ASCII-only case folding.
[[nodiscard]] std::size_t FindCaseInsensitive(std::string_view main_str, std::string_view sub_str) noexcept;
[[nodiscard]] bool IEquals(std::string_view lhs, std::string_view rhs) noexcept;
[[nodiscard]] std::string_view TrimSpaces(std::string_view str) noexcept;
[[nodiscard]] std::string_view StripEdgeChar(std::string_view str, char c) noexcept;
