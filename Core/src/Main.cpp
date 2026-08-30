#include "SingletonData.h"
#include "Macros.hpp"

#include "AnalysisCGNS.h"

#include <cstdlib>
#include <exception>
#include <filesystem>

int main(int argc, char* argv[])
{
    if (!SINGLE_DATA.ProcessArguments(argc, argv)) {
        return EXIT_FAILURE;
    }
    SCOPED_TIMER_LOG("Main");

    if (!std::filesystem::exists(INPUT_PATH)) {
        LOG_ERROR("Input path [{}] does not exist.", INPUT_PATH);
        return -1;
    }

    try {
        AnalysisCGNS analysis;
        if (!analysis || !analysis.Analyze(INPUT_PATH)) {
            return EXIT_FAILURE;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("ReaderCGNS failed: {}", e.what());
    }
    catch (...) {
        LOG_ERROR("Unknown exception");
    }

    return 0;
}
