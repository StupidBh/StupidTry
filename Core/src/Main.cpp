#include "Function.h"
#include "SingletonData.hpp"

#include "CgnsCore.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER(std::filesystem::path(argv[0]).filename().string());
    SINGLE_DATA.VM = ProcessArguments(argc, argv);

    CallCmd("ping www.bilibili.com", true);

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}
