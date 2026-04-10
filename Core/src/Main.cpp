#include "Function.h"
#include "SingletonData.h"

#include "H5Core.h"
#include "CgnsCore.h"

int main(int argc, char* argv[])
{
    SCOPED_TIMER("stupid-try main");
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    cgns::InitLog(LOG);
    cgns::OpenCGNS(INPUT_PATH);

    stupid_h5::InitLog(LOG);
    stupid_h5::TestH5();
    stupid_h5::TestHighFive();

    spdlog::shutdown();
    return 0;
}
