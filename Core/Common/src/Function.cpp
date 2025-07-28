#include "Function.h"

#include "log/logger.hpp"

boost::program_options::variables_map ProcessArguments(int argc, char* argv[])
{
    namespace po = boost::program_options;

    // --getInfo --inputPath testPath --outputPath .\Test\config\info.json
    // --trans -i testPath -c .\Test\config\info.json -w .\Test -n 8 -d 16 --NonEn --DEBUG

    po::options_description desc("Usage: [options]", 150, 10);
    desc.add_options()(
        "help",
        "Produce help message")("getInfo", po::bool_switch()->default_value(false), "Execution mode for get info.json") //
        ("trans", po::bool_switch()->default_value(false), "Execution mode for trans")                                  //
        ("inputPath,i", po::value<std::string>()->required(), "Input file path (required)")                             //
        ("outputPath,o", po::value<std::string>(), "Output json file path (required by 'getInfo')")                     //
        ("configPath,c", po::value<std::string>(), "Input json file path")                                              //
        ("workDirectory,w", po::value<std::string>(), "Work directory")                                                 //
        ("cpuNum,n", po::value<int>()->default_value(8), "Number of CPUs")                                              //
        ("bitDepth,d", po::value<int>()->default_value(32), "Data precision")                                           //
        ("En", po::bool_switch()->default_value(false), "Perform the conversion mode 'En'")                             //
        ("NonEn", po::bool_switch()->default_value(false), "Perform the conversion mode 'NonEn'")                       //
        ("DEBUG", po::bool_switch()->default_value(false), "Enable verbose output")                                     //
        ;

    std::ostringstream oss;
    oss << desc;

    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        if (vm.contains("help")) {
            LOG_INFO("{}", oss.str());
            return {};
        }
        po::notify(vm);
    }
    catch (const po::error& e) {
        LOG_ERROR("Parameter error: {}", e.what());
        LOG_INFO(oss.str());
        return {};
    }
    catch (const std::exception& e) {
        LOG_ERROR(e.what());
        LOG_INFO(oss.str());
        return {};
    }
    catch (...) {
        LOG_ERROR("Unknown error in ProcessArguments");
        LOG_INFO(oss.str());
        return {};
    }

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
    // 安全属性结构，用于允许管道句柄继承
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    // 创建用于读子进程回显消息的管道
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        std::cerr << "CreatePipe failed.\n";
        return;
    }

    // 防止子进程继承读取句柄，导致无法关闭（只继承写入）
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // 设置启动信息，重定向输出
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOA si = { 0 };
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.cb = sizeof(si);

    // 创建子进程
    if (!CreateProcessA(
            nullptr,                            // 不指定应用程序名，直接从命令行解析
            const_cast<char*>(command.c_str()), // 命令行参数（必须可修改）
            nullptr,                            // 进程安全属性
            nullptr,                            // 线程安全属性
            TRUE,                               // 继承句柄
            CREATE_NO_WINDOW,                   // 不显示窗口
            nullptr,                            // 使用父进程的环境变量
            nullptr,                            // 使用父进程的工作目录
            &si,                                // 指向 STARTUPINFO 结构体的指针, 启动信息
            &pi                                 // 指向 PROCESS_INFORMATION 结构体的指针, 进程线程的信息输出
            )) {
        // 清理进程和线程的句柄
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // 关闭子进程的读取端、写入端
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);

        LOG_ERROR("CreateProcess failed: {}", GetLastError());
        LOG_ERROR("Command Line: [{}]", command);
        return;
    }
    CloseHandle(hWritePipe); // 关闭子进程不再需要的写入端

    // 读取子进程的回显消息
    char buffer[4096] = {};
    DWORD bytesRead = 0;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';

        std::string line(buffer, bytesRead);
        if (line.empty() || std::ranges::all_of(line, [](unsigned char c) { return std::isspace(c); })) {
            continue;
        }
        line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char c) { return c == '\r' || c == '\n'; }), line.end());
        if (IsLikelyGBK(line)) {
            line = GBKToUTF8(line);
        }
        LOG_INFO(line);
    }
    // 等待进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
}

