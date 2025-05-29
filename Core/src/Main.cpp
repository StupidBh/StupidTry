#include <limits>

#include "Utils.hpp"
#include "logger/logger.hpp"

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

    constexpr auto MAX = std::numeric_limits<double>::max();
    LOG_INFO("{}_max: {}", typeid(MAX).name(), MAX);

    return 0;
}
