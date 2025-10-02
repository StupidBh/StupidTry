#include "Function.h"
#include "SingletonData.hpp"

#include "CgnsCore.h"

#include <thread>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SINGLE_DATA.VM = ProcessArguments(argc, argv);
    if (SINGLE_DATA.VM.empty()) {
        return EXIT_FAILURE;
    }
    SCOPED_TIMER(std::filesystem::path(argv[0]).filename().string());
    cgns::InitLog(LOG);

    std::jthread jt(CallCmd, "ping www.bilibili.com", true);

    // std::string cgns_path = "C:\\Sundy\\numeca_result\\CO2_1_Air_adf.cgns";
    std::string cgns_path = "C:\\Sundy\\numeca_result\\CO2_1_Air_hdf5.cgns";
    cgns::OpenCGNS(cgns_path);

    return 0;
}
