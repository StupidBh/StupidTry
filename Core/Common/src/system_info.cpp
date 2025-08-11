#include "system_info.h"

#include <array>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
#endif

size_t get_core_count(CoreType type)
{
#ifdef _WIN32
    auto run_cmd = [](const char* cmd) -> std::string {
        std::array<char, 128> buffer {};
        std::string output;
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
        if (!pipe) {
            throw std::runtime_error("_popen() failed!");
        }

        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
            output += buffer.data();
        }
        return output;
    };

    std::string result;
    switch (type) {
    case CoreType::Physical: result = run_cmd("wmic cpu get NumberOfCores /value"); break;
    case CoreType::Logical :
    case CoreType::Total   : result = run_cmd("wmic cpu get NumberOfLogicalProcessors /value"); break;
    }

    // 从输出提取数字
    auto pos = result.find('=');
    if (pos == std::string::npos) {
        throw std::runtime_error("Unexpected WMIC output: " + result);
    }
    return static_cast<size_t>(std::stoi(result.substr(pos + 1)));

#elif defined(__linux__)
    if (type == CoreType::Logical || type == CoreType::Total) {
        return static_cast<size_t>(std::thread::hardware_concurrency());
    }
    // 物理核心数：解析 /proc/cpuinfo
    std::array<char, 128> buffer {};
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen("lscpu | grep 'Core(s) per socket' | awk '{print $4}'", "r"),
        pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    if (!fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
        throw std::runtime_error("Failed to read lscpu output");
    }

    size_t cores_per_socket = static_cast<size_t>(std::stoi(buffer.data()));
    // 获取 socket 数
    pipe.reset(popen("lscpu | grep 'Socket(s)' | awk '{print $2}'", "r"));
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    if (!fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
        throw std::runtime_error("Failed to read lscpu output");
    }
    size_t sockets = static_cast<size_t>(std::stoi(buffer.data()));

    return cores_per_socket * sockets;

#elif defined(__APPLE__)
    auto run_sysctl = [](const char* name) -> int {
        std::array<char, 128> buffer {};
        std::string output;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen((std::string("sysctl -n ") + name).c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        if (!fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get())) {
            throw std::runtime_error("Failed to read sysctl output");
        }
        return std::stoi(buffer.data());
    };

    switch (type) {
    case CoreType::Physical: return static_cast<size_t>(run_sysctl("hw.physicalcpu"));
    case CoreType::Logical :
    case CoreType::Total   : return static_cast<size_t>(run_sysctl("hw.logicalcpu"));
    }
#endif

    throw std::runtime_error("Unsupported platform");
}
