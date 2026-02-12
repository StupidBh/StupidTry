#include "Function.h"
#include "SingletonData.h"

#include "CgnsCore.h"

int main(int argc, char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    cgns::InitLog(LOG);
    std::string cgns_path = "D:\\work\\openfoam-case\\cavity\\CGNS\\cavity_0.cgns";
    cgns::OpenCGNS(cgns_path);

    spdlog::shutdown();
    return 0;
}
