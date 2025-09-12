#include "Function.h"
#include "SingletonData.hpp"

#include "CgnsCore.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main");
    SINGLE_DATA.VM = ProcessArguments(argc, argv);

    int cpu = get_core_count(CoreType::Physical);
    LOG_INFO("PhysicalCore: {}", cpu);

    CallCmd("ping www.bilibili.com", true);

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

