#include "log/logger.hpp"
#include "SingletonData.hpp"
#include "highfive/highfive.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
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

    std::string filename = "./new_file.h5";

    HighFive::File file(filename, HighFive::File::Truncate);
    auto group_tet = file.createGroup("TST");
    group_tet.createAttribute("1", 1);

    SINGLE_DATA.m_pool = std::make_unique<ThreadPool>(10);
    SINGLE_DATA.m_pool->wait_for_completion();

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}
