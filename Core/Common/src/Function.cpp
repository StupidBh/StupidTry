#include "Function.h"

#include "SingletonData.hpp"

boost::program_options::variables_map ProcessArguments(int argc, char* argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("Usage: [options]", 150, 10);
    desc.add_options()("help", "Produce help message")                                //
        ("inputPath,i", po::value<std::string>(), "Input file path (required)")       //
        ("workDirectory,w", po::value<std::string>()->required(), "Work directory")   //
        ("cpuNum,n", po::value<std::uint32_t>()->default_value(8u), "Number of CPUs") //
        ("DEBUG", po::bool_switch()->default_value(false), "Enable verbose output")   //
        ;

    std::ostringstream oss;
    oss << desc;

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.contains("help")) {
            std::cerr << oss.str() << std::endl;
            return {};
        }

        po::notify(vm);
    }
    catch (const po::error& e) {
        std::cerr << "Parameter error: " << e.what() << std::endl;
        std::cerr << oss.str() << std::endl;
        return {};
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << oss.str() << std::endl;
        return {};
    }
    catch (...) {
        std::cerr << "Unknown error in ProcessArguments" << std::endl;
        std::cerr << oss.str() << std::endl;
        return {};
    }

    std::filesystem::path workDirectory = vm["workDirectory"].as<std::string>();
    _Logging_::Logger::get_instance().InitLog(workDirectory, stupid::APP_NAME, vm["DEBUG"].as<bool>());

    return vm;
}

bool IsLikelyGBK(const std::string& str)
{
    for (std::string::size_type i = 0; i < str.length(); ++i) {
        unsigned char c1 = static_cast<unsigned char>(str[i]);
        if (c1 <= 0x7F) { // ASCII
            continue;
        }
        else if (c1 >= 0x81 && c1 <= 0xFE) {
            if (i + 1 >= str.length()) {
                return false; // 不完整的双字节字符
            }

            unsigned char c2 = static_cast<unsigned char>(str[i + 1]);
            if (c2 < 0x40 || c2 > 0xFE || c2 == 0x7F) {
                return false;
            }
            ++i; // GBK 双字节字符
        }
        else {
            return false;
        }
    }
    return true;
}

std::string GBKToUTF8(const std::string& gbk_str)
{
    // GBK -> UTF-16
    int wlen = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, nullptr, 0);
    if (wlen == 0) {
        LOG_WARN("Failed to convert GBK to UTF-16");
        return gbk_str;
    }
    std::wstring wstr(wlen - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, wstr.data(), wlen);

    // UTF-16 -> UTF-8
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_len == 0) {
        LOG_WARN("Failed to convert UTF-16 to UTF-8");
        return gbk_str;
    }
    std::string utf8_str(utf8_len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, utf8_str.data(), utf8_len, nullptr, nullptr);

    return utf8_str;
}

void CallCmd(const std::string& command)
{
    SCOPED_TIMER(std::format("CallCmd: [{}]", command));

    // RAII 管理 HANDLE
    struct HandleCloser
    {
        void operator()(HANDLE h) const
        {
            if (h && h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
            }
        }
    };

    using UniqueHandle = std::unique_ptr<void, HandleCloser>;

    // 安全属性结构，用于允许管道句柄继承
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // 创建用于读子进程回显消息的管道
    HANDLE readPipeRaw = nullptr, writePipeRaw = nullptr;
    if (!CreatePipe(&readPipeRaw, &writePipeRaw, &sa, 0)) {
        std::cerr << "CreatePipe failed.\n";
        return;
    }
    UniqueHandle hReadPipe(readPipeRaw);
    UniqueHandle hWritePipe(writePipeRaw);

    // 防止子进程继承读取句柄，导致无法关闭（只继承写入）
    SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0);

    // 设置启动信息，重定向输出
    PROCESS_INFORMATION pi = {};
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = hWritePipe.get();
    si.hStdError = hWritePipe.get();

    // 创建子进程
    if (!CreateProcessA(
            nullptr,                            // 不指定应用程序名，直接从命令行解析
            const_cast<char*>(command.c_str()), // 命令行参数（必须可修改）
            nullptr,
            nullptr,                            // 安全属性
            TRUE,                               // 继承句柄
            CREATE_NO_WINDOW,                   // 不显示窗口
            nullptr,                            // 使用父进程的环境变量
            nullptr,                            // 使用父进程的工作目录
            &si,                                // 指向 STARTUPINFO 结构体的指针
            &pi                                 // 指向 PROCESS_INFORMATION 结构体的指针
            )) {
        LOG_ERROR("CreateProcess failed: {}", GetLastError());
        LOG_ERROR("Command Line: [{}]", command);
        return;
    }
    hWritePipe.reset(); // 父进程不再需要写入端

    UniqueHandle hProcess(pi.hProcess);
    UniqueHandle hThread(pi.hThread);

    // 读取子进程的回显消息
    char buffer[4096] = {};
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe.get(), buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';

        std::string line(buffer, bytesRead);
        if (line.empty() || std::ranges::all_of(line, [](unsigned char c) { return std::isspace(c); })) {
            continue;
        }
        std::erase_if(line, [](unsigned char c) { return c == '\r' || c == '\n' || c == '\t'; });
        if (IsLikelyGBK(line)) {
            line = GBKToUTF8(line);
        }
        LOG_INFO(line);
    }

    // 等待进程结束
    WaitForSingleObject(hProcess.get(), INFINITE);
}

std::string GetEnv(const std::string& env)
{
    DWORD need_size = GetEnvironmentVariableA(env.c_str(), nullptr, 0);
    if (need_size == 0) {
        LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
        return {};
    }

    std::string buffer(need_size, '\0');
    if (GetEnvironmentVariableA(env.c_str(), buffer.data(), need_size) == 0) {
        LOG_ERROR("GetEnvironmentVariableA [{}] failed: {}", env, GetLastError());
        return {};
    }
    buffer.pop_back(); // 去掉末尾的 '\0'
    return buffer;
}

std::size_t FindCaseInsensitive(const std::string& main_str, const std::string& sub_str)
{
    auto eq_case_insensitive = [](unsigned char ch1, unsigned char ch2) {
        return std::tolower(ch1) == std::tolower(ch2);
    };

    auto it = std::ranges::search(main_str, sub_str, eq_case_insensitive);

    return it.empty() ? std::string::npos : static_cast<std::size_t>(std::distance(main_str.begin(), it.begin()));
}
