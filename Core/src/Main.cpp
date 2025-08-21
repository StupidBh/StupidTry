#include "SingletonData.hpp"
#include "ThreadPool.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main");
    SINGLE_DATA.VM = ProcessArguments(argc, argv);

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}
