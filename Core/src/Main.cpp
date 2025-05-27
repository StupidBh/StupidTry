#include <limits>

#include "Logger.hpp"

int main(int argc, char* argv[])
{
    spdlog::init_thread_pool(32768, 2);

#ifdef _DEBUG
    std::string command;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            command += argv[i];
            command += " ";
        }
        command.pop_back(); // 去除尾部多余的空格字符
        LOG_DEBUG("Command: '{}'", command);
    }
#endif // _DEBUG

    constexpr int int_max = std::numeric_limits<int>::max();
    LOG_INFO("INT_MAX: {}", int_max);

    return 0;
}
