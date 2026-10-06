#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

/// Byte-wise comparisons with ASCII-only case folding.
[[nodiscard]] std::size_t FindCaseInsensitive(std::string_view main_str, std::string_view sub_str) noexcept;
[[nodiscard]] bool IEquals(std::string_view lhs, std::string_view rhs) noexcept;
[[nodiscard]] std::string_view TrimSpaces(std::string_view str) noexcept;
[[nodiscard]] std::string_view StripEdgeChar(std::string_view str, char c) noexcept;

[[nodiscard]] std::string GetEnv(const std::string& env);

[[nodiscard]] std::filesystem::path GetExecutablePath();
[[nodiscard]] std::filesystem::path GetExecutableDirectory();

enum class ExecuteProcessAction
{
    /// Continue reading and delivering command output.
    Continue,
    /// Stop reading, allow a bounded natural-exit window, then terminate the process tree if needed.
    StopReadingAndWait,
    /// Terminate the process tree immediately, then wait for termination to complete.
    TerminateImmediately,
};

struct ExecuteProcessConfig
{
    std::filesystem::path exe_path;
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> environment;
};

void ExecuteProcess(const ExecuteProcessConfig& config, std::function<ExecuteProcessAction(const std::string&)> callback = { });
