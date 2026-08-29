#pragma once
#include <windows.h>

#include <filesystem>
#include <functional>
#include <memory>
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

/// Controls how CallCmd proceeds after delivering an output line to its callback.
enum class CallCmdAction
{
    /// Continue reading and delivering command output.
    Continue,
    /// Stop reading, allow a bounded natural-exit window, then terminate the process tree if needed.
    StopReadingAndWait,
    /// Terminate the process tree immediately, then wait for termination to complete.
    TerminateImmediately,
};

/// Runs a UTF-8 command through cmd.exe and forwards each non-empty, trimmed UTF-8 output line.
void CallCmd(const std::string& command, std::function<CallCmdAction(const std::string&)> callback = { });

/// Returns an environment variable, or an empty string when it cannot be read.
[[nodiscard]] std::string GetEnv(const std::string& env);

[[nodiscard]] std::filesystem::path GetExecutablePath();
[[nodiscard]] std::filesystem::path GetExecutableDirectory();

class ModuleGuard final {
public:
    explicit ModuleGuard(const std::filesystem::path& library_path) noexcept;
    ~ModuleGuard() noexcept;

    ModuleGuard(const ModuleGuard&) = delete;
    ModuleGuard& operator=(const ModuleGuard&) = delete;
    ModuleGuard(ModuleGuard&& other) noexcept;
    ModuleGuard& operator=(ModuleGuard&& other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_module != nullptr; }

    [[nodiscard]] HMODULE GetModule() const noexcept { return this->m_module; }

private:
    HMODULE m_module = nullptr;
};

[[nodiscard]] std::unique_ptr<ModuleGuard> LoadModuleGuard(const std::filesystem::path& library_path);
