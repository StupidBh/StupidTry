#include "SingletonData.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main");
    SINGLE_DATA.m_vm = ProcessArguments(argc, argv);

    CallCmd("ping www.baidu.com");

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

