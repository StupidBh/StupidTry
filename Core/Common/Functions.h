#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

/// Returns true when the complete byte sequence is valid UTF-8.
[[nodiscard]] bool IsValidUTF8(std::string_view str) noexcept;

/// Returns true when the non-ASCII bytes form valid GBK double-byte sequences.
[[nodiscard]] bool IsLikelyGBK(std::string_view str) noexcept;

/// Converts GBK bytes to UTF-8. Returns the original bytes if conversion fails.
[[nodiscard]] std::string GBKToUTF8(std::string_view gbk_str);

/// Preserves valid UTF-8 and converts input that is valid GBK but invalid UTF-8.
[[nodiscard]] std::string NormalizeToUTF8(std::string_view str);

/// Runs a command and forwards each non-empty, trimmed UTF-8 output line.
/// Returning true from the callback requests early process termination.
void CallCmd(const std::string& command, std::function<bool(const std::string&)> callback = { });

/// Returns an environment variable, or an empty string when it cannot be read.
[[nodiscard]] std::string GetEnv(const std::string& env);

/// Byte-wise comparisons with ASCII-only case folding.
[[nodiscard]] std::size_t FindCaseInsensitive(std::string_view main_str, std::string_view sub_str) noexcept;
[[nodiscard]] bool IEquals(std::string_view lhs, std::string_view rhs) noexcept;
[[nodiscard]] std::string_view TrimSpaces(std::string_view str) noexcept;
[[nodiscard]] std::string_view StripEdgeChar(std::string_view str, char c) noexcept;

[[nodiscard]] std::filesystem::path GetExecutablePath();
[[nodiscard]] std::filesystem::path GetExecutableDirectory();
