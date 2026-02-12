#include "Function.h"
#include "SingletonData.h"

#include "highfive/highfive.hpp"

int main(int argc, char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    for (auto& [key, value] : SINGLE_DATA_VM) {
        if (value.empty()) {
            LOG_WARN("<empty>-[{}] = <empty>", key);
        }
        else if (value.value().type() == typeid(std::string)) {
            LOG_INFO("<string>-[{}] = {}", key, value.as<std::string>());
        }
        else if (value.value().type() == typeid(int)) {
            LOG_INFO("<int>-[{}] = {}", key, value.as<int>());
        }
        else if (value.value().type() == typeid(bool)) {
            LOG_INFO("<bool>-[{}] = {}", key, value.as<bool>());
        }
        else {
            LOG_WARN("[{}] = <unhandled type>", key);
        }
    }

    spdlog::shutdown();
    return 0;
}
