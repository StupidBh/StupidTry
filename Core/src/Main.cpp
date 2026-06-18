#include "Functions.h"
#include "SingletonData.h"

#include "CgnsCore.h"
#include "HighFiveUtils.hpp"

int main(int argc, char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    if (CgnsCore cgns; cgns.OpenCGNS(INPUT_PATH)) {
        cgns.info();
    }

    struct Grid
    {
        std::uint32_t ID = 1;
        float X = 1.F;
        float Y = 1.F;
        float Z = 1.F;
        std::string doc = "TryToStringMessage";

        static HighFive::CompoundType CompoundType()
        {
            return HighFive::CompoundType(
                std::vector<HighFive::CompoundType::member_def> {
                    { "ID", HighFive::create_datatype<std::uint32_t>(), offsetof(Grid, ID) }, //
                    { "X", HighFive::create_datatype<float>(), offsetof(Grid, X) },           //
                    { "Y", HighFive::create_datatype<float>(), offsetof(Grid, Y) },           //
                    { "Z", HighFive::create_datatype<float>(), offsetof(Grid, Z) },           //
                    { "DOC", HighFive::create_datatype<std::string>(), offsetof(Grid, doc) },
                },
                sizeof(Grid) //
            );
        }
    };

    std::vector<Grid> coordinates(10000);
    std::vector<int> vec(1000);
    const auto HF_FILE = WORK_DIR_PATH / "Try1.h5";

    {
        SCOPED_TIMER_LOG(std::format("HighFive::Write file: [{}]", HF_FILE.string()));
        HighFive::File file(HF_FILE.string(), HighFive::File::Overwrite);

        try {
            HFUtils::WriteDataSet(file, "Try1", coordinates, Grid::CompoundType());
            HFUtils::WriteDataSet(file, "Try2", vec);
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    {
        utils::DeepClear(coordinates);
        SCOPED_TIMER_LOG("HighFive::Read file");

        HighFive::File file(HF_FILE.string(), HighFive::File::ReadOnly);
        HighFive::DataSet dataset = file.getDataSet("Try1");
        auto dataset_dims = dataset.getDimensions();
        coordinates.resize(dataset_dims.front());

        try {
            dataset.read_raw(coordinates.data(), Grid::CompoundType());
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    CallCmd("ipconfig", [](const std::string_view& line) {
        LOG_INFO(line);
        return false;
    });

    LOG_INFO(GetExecutableDirectory());
    LOG_INFO(GetExecutablePath());

    spdlog::shutdown();
    return 0;
}
