#include "Functions.h"
#include "Logger/logger.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
    constexpr UINT GBK_CODE_PAGE = 936;
    constexpr DWORD GRACEFUL_EXIT_TIMEOUT_MS = 5000;

    struct WinHandleCloser
    {
        void operator()(void* handle) const noexcept
        {
            if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                CloseHandle(handle);
            }
        }
    };

    using UniqueHandle = std::unique_ptr<void, WinHandleCloser>;

    constexpr unsigned char ToAsciiLower(unsigned char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
    }

    std::optional<std::wstring> MultiByteToWide(UINT code_page, DWORD flags, std::string_view str)
    {
        if (str.empty()) {
            return std::wstring { };
        }
        if (str.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return std::nullopt;
        }

        const int input_size = static_cast<int>(str.size());
        const int wide_size = MultiByteToWideChar(code_page, flags, str.data(), input_size, nullptr, 0);
        if (wide_size <= 0) {
            return std::nullopt;
        }

        std::wstring result(wide_size, L'\0');
        if (MultiByteToWideChar(code_page, flags, str.data(), input_size, result.data(), wide_size) != wide_size) {
            return std::nullopt;
        }
        return result;
    }

    std::optional<std::string> WideToUTF8(std::wstring_view str)
    {
        if (str.empty()) {
            return std::string { };
        }
        if (str.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            SetLastError(ERROR_INVALID_PARAMETER);
            return std::nullopt;
        }

        const int input_size = static_cast<int>(str.size());
        const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, str.data(), input_size, nullptr, 0, nullptr, nullptr);
        if (utf8_size <= 0) {
            return std::nullopt;
        }

        std::string result(utf8_size, '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, str.data(), input_size, result.data(), utf8_size, nullptr, nullptr) != utf8_size) {
            return std::nullopt;
        }
        return result;
    }

    UniqueHandle CreateKillOnCloseJob()
    {
        UniqueHandle job(CreateJobObjectW(nullptr, nullptr));
        if (!job) {
            LOG_ERROR("CreateJobObjectW failed: {}", GetLastError());
            return { };
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = { };
        limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info))) {
            LOG_ERROR("SetInformationJobObject failed: {}", GetLastError());
            return { };
        }

        return job;
    }
} // namespace

bool IsValidUTF8(const std::string_view str) noexcept
{
    if (str.empty()) {
        return true;
    }
    if (str.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }

    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0) > 0;
}

bool IsLikelyGBK(const std::string_view str) noexcept
{
    bool has_high_bit = false;
    for (std::size_t i = 0; i < str.length(); ++i) {
        const unsigned char c1 = static_cast<unsigned char>(str[i]);
        if (c1 <= 0x7F) {
            continue;
        }

        has_high_bit = true;
        if (c1 >= 0x81 && c1 <= 0xFE) {
            if (i + 1 >= str.length()) {
                return false; // 截断
            }

            if (const auto c2 = static_cast<unsigned char>(str[i + 1]); c2 < 0x40 || c2 > 0xFE || c2 == 0x7F) {
                return false;
            }
            ++i;
        }
        else {
            return false; // 出现非法字节
        }
    }
    return has_high_bit;
}

std::string GBKToUTF8(const std::string_view gbk_str)
{
    if (gbk_str.empty()) {
        return { };
    }

    const auto wide = MultiByteToWide(GBK_CODE_PAGE, 0, gbk_str);
    if (!wide) {
        return std::string(gbk_str);
    }

    const auto utf8 = WideToUTF8(*wide);
    return utf8.value_or(std::string(gbk_str));
}

std::string NormalizeToUTF8(const std::string_view str)
{
    return !IsValidUTF8(str) && IsLikelyGBK(str) ? GBKToUTF8(str) : std::string(str);
}

void CallCmd(const std::string& command, std::function<bool(const std::string&)> callback)
{
    if (command.empty()) {
        LOG_ERROR("Command is empty.");
        return;
    }

    // 安全属性结构，用于允许管道句柄继承
    SECURITY_ATTRIBUTES sa = { .nLength = sizeof(SECURITY_ATTRIBUTES), .lpSecurityDescriptor = nullptr, .bInheritHandle = TRUE };

    // 创建用于读子进程回显消息的管道
    HANDLE readPipeRaw = nullptr, writePipeRaw = nullptr;
    if (!CreatePipe(&readPipeRaw, &writePipeRaw, &sa, 0)) {
        const DWORD err = GetLastError();
        LOG_ERROR("CreatePipe failed: {}", err);
        return;
    }
    UniqueHandle hReadPipe(readPipeRaw);
    UniqueHandle hWritePipe(writePipeRaw);

    // 防止子进程继承读取句柄，导致无法关闭（只继承写入）
    if (!SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0)) {
        const DWORD err = GetLastError();
        LOG_ERROR("SetHandleInformation failed: {}", err);
        return;
    }

    // 使用 Job Object 托管进程树，避免强制退出时遗留子进程
    auto hJob = CreateKillOnCloseJob();
    if (!hJob) {
        return;
    }

    // 设置启动信息，重定向输出
    PROCESS_INFORMATION pi = { };
    STARTUPINFOW si { .cb = sizeof(STARTUPINFOW), .dwFlags = STARTF_USESTDHANDLES, .hStdOutput = hWritePipe.get(), .hStdError = hWritePipe.get() };

    // 编码转换：ACP -> UTF-16，正确处理非 ASCII 路径
    auto wide_command = MultiByteToWide(CP_ACP, 0, command);
    if (!wide_command) {
        LOG_ERROR("MultiByteToWideChar failed: {}", GetLastError());
        return;
    }
    std::wstring cmd = std::move(*wide_command);

    if (!CreateProcessW(nullptr,                             // 不指定应用程序名，直接从命令行解析
                        cmd.data(),                          // 命令行参数（必须可修改）
                        nullptr,
                        nullptr,                             // 安全属性
                        TRUE,                                // 继承句柄
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, // 先挂起，确保加入 Job 后再运行
                        nullptr,                             // 使用父进程的环境变量
                        nullptr,                             // 使用父进程的工作目录
                        &si,                                 // 指向 STARTUPINFO 结构体的指针
                        &pi                                  // 指向 PROCESS_INFORMATION 结构体的指针
                        )) {
        const DWORD err = GetLastError();
        LOG_ERROR("CreateProcess failed: {}", err);
        LOG_ERROR("Command Line: [{}].", command);
        return;
    }

    // 先包装句柄，再处理后续逻辑（确保异常安全）
    const UniqueHandle hProcess(pi.hProcess);
    const UniqueHandle hThread(pi.hThread);

    if (!AssignProcessToJobObject(hJob.get(), hProcess.get())) {
        const DWORD err = GetLastError();
        LOG_ERROR("AssignProcessToJobObject failed: {}", err);
        TerminateProcess(hProcess.get(), 1);
        WaitForSingleObject(hProcess.get(), GRACEFUL_EXIT_TIMEOUT_MS);
        return;
    }

    if (ResumeThread(hThread.get()) == static_cast<DWORD>(-1)) {
        const DWORD err = GetLastError();
        LOG_ERROR("ResumeThread failed: {}", err);
        TerminateJobObject(hJob.get(), 1);
        WaitForSingleObject(hProcess.get(), GRACEFUL_EXIT_TIMEOUT_MS);
        return;
    }

    hWritePipe.reset();

    // 读取子进程的回显消息，按行切分（正确处理跨读取块的行）
    std::array<char, 4096> buffer { };
    bool early_exit = false;
    std::string pending; // 尚未遇到换行符的跨块残留

    auto process_line = [&](std::string_view raw) -> bool {
        const std::string_view line_view = TrimSpaces(raw);
        if (line_view.empty()) {
            return false;
        }

        const std::string line = NormalizeToUTF8(line_view);
        if (callback) {
            return callback(line);
        }
        std::cerr << line << '\n';
        return false;
    };

    while (true) {
        DWORD bytes_read = 0;
        if (!ReadFile(hReadPipe.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr)) {
            const DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE) {
                LOG_ERROR("ReadFile failed: {}", error);
            }
            break;
        }
        if (bytes_read == 0) {
            break;
        }
        pending.append(buffer.data(), bytes_read);

        std::size_t start = 0;
        for (std::size_t nl; (nl = pending.find('\n', start)) != std::string::npos; start = nl + 1) {
            if (process_line(std::string_view(pending).substr(start, nl - start))) {
                early_exit = true;
                break;
            }
        }
        pending.erase(0, start);

        if (early_exit) {
            break;
        }
    }

    // 处理最后一行（子进程输出未以换行结尾的残留）
    if (!early_exit && !pending.empty() && process_line(pending)) {
        early_exit = true;
    }

    if (early_exit) {
        hReadPipe.reset(); // 关闭读端；子进程后续写 stdout/stderr 时通常会收到 ERROR_BROKEN_PIPE
        LOG_INFO("Callback requested early termination, waiting for process to exit...");

        const DWORD waitResult = WaitForSingleObject(hProcess.get(), GRACEFUL_EXIT_TIMEOUT_MS);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARN("Process did not exit gracefully after {} ms, terminating job.", GRACEFUL_EXIT_TIMEOUT_MS);
            if (!TerminateJobObject(hJob.get(), 1)) {
                LOG_ERROR("TerminateJobObject failed: {}", GetLastError());
            }

            const DWORD forcedWaitResult = WaitForSingleObject(hProcess.get(), GRACEFUL_EXIT_TIMEOUT_MS);
            if (forcedWaitResult == WAIT_TIMEOUT) {
                LOG_ERROR("Process still alive after TerminateJobObject.");
            }
            else if (forcedWaitResult == WAIT_FAILED) {
                LOG_ERROR("WaitForSingleObject failed after TerminateJobObject: {}", GetLastError());
            }
        }
        else if (waitResult == WAIT_FAILED) {
            LOG_ERROR("WaitForSingleObject failed: {}", GetLastError());
        }
    }
    else {
        const DWORD waitResult = WaitForSingleObject(hProcess.get(), INFINITE);
        if (waitResult == WAIT_FAILED) {
            LOG_ERROR("WaitForSingleObject failed: {}", GetLastError());
        }
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess.get(), &exitCode)) {
        LOG_INFO("Command exited with code {}.", exitCode);
    }
    else {
        LOG_ERROR("GetExitCodeProcess failed: {}", GetLastError());
    }
}

std::string GetEnv(const std::string& env)
{
    if (env.empty()) {
        LOG_ERROR("Environment variable name is empty.");
        return { };
    }

    DWORD buffer_size = GetEnvironmentVariableA(env.c_str(), nullptr, 0);
    if (buffer_size == 0) {
        LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
        return { };
    }

    std::string buffer(buffer_size, '\0');
    while (true) {
        const DWORD value_size = GetEnvironmentVariableA(env.c_str(), buffer.data(), static_cast<DWORD>(buffer.size()));
        if (value_size == 0) {
            LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
            return { };
        }
        if (value_size < buffer.size()) {
            buffer.resize(value_size);
            return buffer;
        }

        buffer.resize(value_size);
    }
}

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

std::filesystem::path GetExecutablePath()
{
    std::wstring buffer(256, L'\0');

    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            LOG_ERROR("GetModuleFileNameW failed: {}", GetLastError());
            return { };
        }

        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }

        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path GetExecutableDirectory()
{
    return GetExecutablePath().parent_path();
}
