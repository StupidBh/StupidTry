#include "SingletonData.h"
#include "Macros.hpp"

#include "ReaderAPI/ReaderCGNS.h"
#include "HighFiveUtils.hpp"
#include "ReaderCGNSLogGuard.h"

#include <memory>
#include <windows.h>

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
        struct ModuleGuard
        {
            HMODULE module = nullptr;

            ~ModuleGuard()
            {
                if (module != nullptr) {
                    FreeLibrary(module);
                }
            }
        };

        ModuleGuard module_guard { LoadLibraryW(L"ReaderCGNS.dll") };
        if (module_guard.module == nullptr) {
            LOG_ERROR("Failed to load ReaderCGNS.dll: {}", GetLastError());
            return EXIT_FAILURE;
        }

        const ReaderCGNSLogGuard reader_cgns_log_guard(module_guard.module);
        if (!reader_cgns_log_guard) {
            LOG_ERROR("Failed to install the ReaderCGNS log callback.");
            return EXIT_FAILURE;
        }

        auto create = reinterpret_cast<ReaderAPI::CreateReaderCGNSFunc>(GetProcAddress(module_guard.module, "CreateReaderCGNS"));
        if (create == nullptr) {
            LOG_ERROR("ReaderCGNS.dll does not export CreateReaderCGNS: {}", GetLastError());
            return EXIT_FAILURE;
        }

        auto destroy = reinterpret_cast<ReaderAPI::DestroyReaderCGNSFunc>(GetProcAddress(module_guard.module, "DestroyReaderCGNS"));
        if (destroy == nullptr) {
            LOG_ERROR("ReaderCGNS.dll does not export DestroyReaderCGNS: {}", GetLastError());
            return EXIT_FAILURE;
        }

        std::unique_ptr<ReaderAPI::ReaderCGNS, ReaderAPI::DestroyReaderCGNSFunc> reader(create(), destroy);
        if (reader->Open(INPUT_PATH)) {
            reader->info();
            reader->Close();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("ReaderCGNS failed: {}", e.what());
    }
    catch (...) {
        LOG_ERROR("Unknown exception");
    }

    const auto HF_FILE = WORK_DIR_PATH / "Try1.h5";

    struct Grid
    {
        std::uint32_t ID = 0;
        float X = 0.F;
        float Y = 0.F;
        float Z = 0.F;
        const char* doc = "NULL";

        static HighFive::CompoundType CompoundType()
        {
            return HighFive::CompoundType(
                std::vector<HighFive::CompoundType::member_def> {
                    { "ID", HighFive::create_datatype<std::uint32_t>(), offsetof(Grid, ID) }, //
                    { "X", HighFive::create_datatype<float>(), offsetof(Grid, X) },           //
                    { "Y", HighFive::create_datatype<float>(), offsetof(Grid, Y) },           //
                    { "Z", HighFive::create_datatype<float>(), offsetof(Grid, Z) },           //
                    { "DOC", HighFive::VariableLengthStringType(), offsetof(Grid, doc) },     //
                },
                sizeof(Grid));
        }
    };

    std::vector<Grid> coordinates;

    {
        std::size_t VALUE_SUM = 100000;
        std::vector<int> vec;
        std::vector<std::string> grid_docs;
        grid_docs.reserve(VALUE_SUM);
        for (std::uint32_t i = 0; i < VALUE_SUM; ++i) {
            float x = 1.F + static_cast<float>(i);
            float y = 2.F + static_cast<float>(i);
            float z = 3.F + static_cast<float>(i);
            grid_docs.emplace_back(std::format("{}-[{},{},{}]", i, x, y, z));
            vec.emplace_back(i);

            coordinates.emplace_back(Grid { .ID = i,
                                            .X = x,
                                            .Y = y,
                                            .Z = z, //
                                            .doc = grid_docs.back().c_str() });
        }

        SCOPED_TIMER_LOG(std::format("HighFive::Write file: [{}]", HF_FILE.string()));
        HighFive::File file(HF_FILE.string(), HighFive::File::Truncate);

        try {
            std::jthread task_1([&] { HFUtils::WriteDataSet(file, "Try1", vec); });
            std::jthread task_2([&] { HFUtils::WriteDataSet(file, "Try2", coordinates, Grid::CompoundType()); });

            std::jthread task_3([&] {
                HFUtils::WriteDataSetAppendable(file, "Try3", coordinates, Grid::CompoundType());
                HFUtils::WriteDataSetAppendable(file, "Try3", coordinates, Grid::CompoundType());
            });
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

        try {
            HFUtils::ReadDataSet(file.getDataSet("Try2"), coordinates, Grid::CompoundType());
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    // test_thread.join();
    return 0;
}
