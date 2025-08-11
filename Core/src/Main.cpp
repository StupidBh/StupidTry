#include "log/logger.hpp"
#include "SingletonData.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main-function");
    SINGLE_DATA.m_vm = ProcessArguments(argc, argv);

    CallCmd("ping www.baidu.com");

    try {
        LOG_INFO("Physical-Cores: {}", get_core_count(CoreType::Physical));
        LOG_INFO("Logical-Cores: {}", get_core_count(CoreType::Logical));
        LOG_INFO("Total-Cores: {}", get_core_count(CoreType::Total));
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

