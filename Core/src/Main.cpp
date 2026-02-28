#include "Function.h"
#include "SingletonData.h"

#include "CgnsCore.h"
#include "HighFive.hpp"

int main(int argc, char* argv[])
{
    SCOPED_TIMER("stupid-try main");
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    cgns::InitLog(LOG);
    cgns::OpenCGNS(INPUT_PATH);

    std::vector<float> output_data(1'000'000'000, 2.F);
    const std::string H5_FILE = "./Try.h5";
    HighFive::File file(H5_FILE, HighFive::File::Overwrite);
    {
        SCOPED_TIMER("WriteDataSet");
        WriteDataSet("Try", output_data, file);
        WriteDataSet("Try", std::move(output_data), file.createGroup("TryGroup"));
    }

    spdlog::shutdown();
    return 0;
}
