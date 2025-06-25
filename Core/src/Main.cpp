#include "Utils.hpp"
#include "log/logger.hpp"

#include "boost/process.hpp"

int main(int argc, char* argv[])
{
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
    else {
        LOG_DEBUG("No command line arguments provided.");
    }
#endif // _DEBUG

    return 0;
}
