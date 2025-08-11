#include "log/logger.hpp"
#include "SingletonData.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    utils::ScopedTimer timer("main-function", [&](std::string_view msg) { LOG_INFO("{}", msg); });
    SINGLE_DATA.m_vm = ProcessArguments(argc, argv);

    CallCmd("ping bing.com");

    try {
        LOG_INFO("Physical cores: {}", get_core_count(CoreType::Physical));
        LOG_INFO("Logical cores: {}", get_core_count(CoreType::Logical));
        LOG_INFO("Total cores: {}", get_core_count(CoreType::Total));
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

