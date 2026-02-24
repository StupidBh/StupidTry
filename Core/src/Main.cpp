#include "Function.h"
#include "SingletonData.h"

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

    spdlog::shutdown();
    return 0;
}
