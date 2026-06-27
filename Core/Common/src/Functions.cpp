#include "Functions.h"

#include <windows.h>

bool IsLikelyGBK(const std::string_view str)
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

    int wlen = MultiByteToWideChar(936, 0, gbk_str.data(), static_cast<int>(gbk_str.size()), nullptr, 0);
    if (wlen <= 0) {
        return std::string(gbk_str);
    }

    std::wstring wstr(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk_str.data(), static_cast<int>(gbk_str.size()), wstr.data(), wlen);
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) {
        return std::string(gbk_str);
    }

    std::string utf8_str(u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wlen, utf8_str.data(), u8len, nullptr, nullptr);

    return utf8_str;
}

void CallCmd(const std::string& command, std::function<bool(const std::string&)> callback)
{
    constexpr DWORD graceful_exit_timeout_ms = 5000;

    // 安全属性结构，用于允许管道句柄继承
    SECURITY_ATTRIBUTES sa = { .nLength = sizeof(SECURITY_ATTRIBUTES),
                               .lpSecurityDescriptor = nullptr,
                               .bInheritHandle = TRUE };

    // 创建用于读子进程回显消息的管道
    HANDLE readPipeRaw = nullptr, writePipeRaw = nullptr;
    if (!CreatePipe(&readPipeRaw, &writePipeRaw, &sa, 0)) {
        const DWORD err = GetLastError();
        LOG_ERROR("CreatePipe failed: {}", err);
        return;
    }
    auto hReadPipe = std::shared_ptr<void>(readPipeRaw, CloseHandle);
    auto hWritePipe = std::shared_ptr<void>(writePipeRaw, CloseHandle);

    // 防止子进程继承读取句柄，导致无法关闭（只继承写入）
    if (!SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0)) {
        const DWORD err = GetLastError();
        LOG_ERROR("SetHandleInformation failed: {}", err);
        return;
    }

    // 使用 Job Object 托管进程树，避免强制退出时遗留子进程
    auto make_job = []() -> std::shared_ptr<void> {
        const HANDLE raw = CreateJobObjectW(nullptr, nullptr);
        if (raw == nullptr) {
            LOG_ERROR("CreateJobObjectW failed: {}", GetLastError());
            return { };
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = { };
        limit_info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(raw, JobObjectExtendedLimitInformation, &limit_info, sizeof(limit_info))) {
            LOG_ERROR("SetInformationJobObject failed: {}", GetLastError());
            CloseHandle(raw);
            return { };
        }

        return { raw, CloseHandle };
    };

    auto hJob = make_job();
    if (hJob == nullptr) {
        return;
    }

    // 设置启动信息，重定向输出
    PROCESS_INFORMATION pi = { };
    STARTUPINFOW si { .cb = sizeof(STARTUPINFOW),
                      .dwFlags = STARTF_USESTDHANDLES,
                      .hStdOutput = hWritePipe.get(),
                      .hStdError = hWritePipe.get() };

    // 编码转换：ACP -> UTF-16，正确处理非 ASCII 路径
    int wlen = MultiByteToWideChar(CP_ACP, 0, command.data(), static_cast<int>(command.size()), nullptr, 0);
    if (wlen <= 0) {
        LOG_ERROR("MultiByteToWideChar failed: {}", GetLastError());
        return;
    }
    std::wstring cmd(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, command.data(), static_cast<int>(command.size()), cmd.data(), wlen);

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
    const auto hProcess = std::shared_ptr<void>(pi.hProcess, CloseHandle);
    auto hThread = std::shared_ptr<void>(pi.hThread, CloseHandle);

    if (!AssignProcessToJobObject(hJob.get(), hProcess.get())) {
        const DWORD err = GetLastError();
        LOG_ERROR("AssignProcessToJobObject failed: {}", err);
        TerminateProcess(hProcess.get(), 1);
        WaitForSingleObject(hProcess.get(), graceful_exit_timeout_ms);
        return;
    }

    if (ResumeThread(hThread.get()) == static_cast<DWORD>(-1)) {
        const DWORD err = GetLastError();
        LOG_ERROR("ResumeThread failed: {}", err);
        TerminateJobObject(hJob.get(), 1);
        WaitForSingleObject(hProcess.get(), graceful_exit_timeout_ms);
        return;
    }

    hWritePipe.reset();

    // 读取子进程的回显消息，按行切分（正确处理跨读取块的行）
    char buffer[4096] = { };
    DWORD bytesRead = 0;
    bool early_exit = false;
    std::string pending; // 尚未遇到换行符的跨块残留

    auto process_line = [&](std::string_view raw) -> bool {
        const std::string_view line_view = TrimSpaces(raw);
        if (line_view.empty()) {
            return false;
        }
        if (callback != nullptr) {
            return callback(std::string(line_view));
        }
        std::cerr << line_view << '\n';
        return false;
    };

    while (ReadFile(hReadPipe.get(), buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        pending.append(buffer, bytesRead);

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

        const DWORD waitResult = WaitForSingleObject(hProcess.get(), graceful_exit_timeout_ms);
        if (waitResult == WAIT_TIMEOUT) {
            LOG_WARN("Process did not exit gracefully after {} ms, terminating job.", graceful_exit_timeout_ms);
            if (!TerminateJobObject(hJob.get(), 1)) {
                LOG_ERROR("TerminateJobObject failed: {}", GetLastError());
            }

            const DWORD forcedWaitResult = WaitForSingleObject(hProcess.get(), graceful_exit_timeout_ms);
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
    const DWORD need_size = GetEnvironmentVariableA(env.c_str(), nullptr, 0);
    if (need_size == 0) {
        LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
        return { };
    }

    std::string buffer(need_size, '\0');
    if (GetEnvironmentVariableA(env.c_str(), buffer.data(), need_size) == 0) {
        LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
        return { };
    }
    buffer.pop_back(); // 去掉末尾的 '\0'
    return buffer;
}

std::size_t FindCaseInsensitive(std::string_view haystack, std::string_view needle)
{
    if (needle.empty()) {
        return 0;
    }

    const auto res = std::ranges::search(haystack, needle, [](unsigned char c1, unsigned char c2) {
        return (c1 == c2) || (std::tolower(c1) == std::tolower(c2));
    });
    if (res.begin() == haystack.end()) {
        return std::string_view::npos;
    }

    return static_cast<std::size_t>(std::ranges::distance(haystack.begin(), res.begin()));
}

bool IEquals(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    return std::ranges::equal(lhs, rhs, [](unsigned char c1, unsigned char c2) {
        return (c1 == c2) || (std::tolower(c1) == std::tolower(c2));
    });
}

std::string_view TrimSpaces(std::string_view sv)
{
    static constinit std::string_view whitespace_chars = " \t\n\r\f\v";
    auto first = sv.find_first_not_of(whitespace_chars);
    if (first == std::string_view::npos) {
        return { };
    }
    sv.remove_prefix(first);

    const auto last = sv.find_last_not_of(whitespace_chars);
    if (last == std::string_view::npos) {
        return sv;
    }
    sv.remove_suffix(sv.size() - last - 1);

    return sv;
}

std::string_view TrimQuotes(std::string_view sv)
{
    if (sv.size() < 2) {
        return sv;
    }
    if (sv.front() == '"') {
        sv.remove_prefix(1);
    }
    if (sv.back() == '"') {
        sv.remove_suffix(1);
    }
    return sv;
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
