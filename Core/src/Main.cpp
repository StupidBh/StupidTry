#include "log/logger.hpp"

#include "Utils.hpp"
#include "SingletonData.hpp"
#include "SyncIO.h"

#include <queue>

int main(int argc, char* argv[])
{
#ifdef _DEBUG
    if (argc > 1) {
        std::string command;
        for (int i = 1; i < argc; ++i) {
            command += argv[i];
            command += " ";
        }
        command.pop_back(); // 去除尾部多余的空格字符
        std::cout << command << std::endl;
    }
#endif

    SyncIO().run();

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}
