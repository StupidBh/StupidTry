#include "log/logger.hpp"
#include "SingletonData.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SINGLE_DATA.m_vm = ProcessArguments(argc, argv);

    CallCmd("ping google.com");


    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

