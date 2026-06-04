#include "Functions.h"
#include "SingletonData.h"

#include "CgnsCore.h"

#include "Logger/logger.hpp"

int main(int argc, char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    CgnsCore cgns(INPUT_PATH);
    if (!cgns.OpenCGNS()) {
        return EXIT_FAILURE;
    }
    cgns.info();

    spdlog::shutdown();
    return 0;
}
