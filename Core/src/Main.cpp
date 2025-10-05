#include "Function.h"
#include "SingletonData.hpp"

#include "CgnsCore.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SINGLE_DATA.VM = ProcessArguments(argc, argv);
    if (SINGLE_DATA.VM.empty()) {
        return EXIT_FAILURE;
    }
    SCOPED_TIMER(std::filesystem::path(argv[0]).filename().string());

    cgns::InitLog(LOG);

    LOG_INFO("STUPID_VER_MAJOR: {}", stupid::STUPID_VER_MAJOR);
    LOG_WARN("STUPID_VER_MINOR: {}", stupid::STUPID_VER_MINOR);
    LOG_DEBUG("STUPID_VER_PATCH: {}", stupid::STUPID_VER_PATCH);
    LOG_ERROR("STUPID_VERSION: {}", stupid::STUPID_VERSION);

    return 0;
}
