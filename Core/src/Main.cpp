#include "SingletonData.h"
#include "TimerMacros.h"

#include "ReaderCGNS/ReaderCGNS.h"
#include "HighFiveUtils.hpp"
#include "ReaderCGNSLogGuard.h"

#include <string_view>

static void
    ReaderCGNSLogCallback(void* context, const ReaderCGNS::Logger::ReaderCGNS_LogLevel level, const char* file, const int line, const char* message)
{
    auto* logger = static_cast<spdlog::logger*>(context);
    if (logger == nullptr) {
        return;
    }

    spdlog::level::level_enum spd_level;
    switch (level) {
    case ReaderCGNS::Logger::READER_CGNS_LOG_TRACE   : spd_level = spdlog::level::trace; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_DEBUG   : spd_level = spdlog::level::debug; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_INFO    : spd_level = spdlog::level::info; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_WARN    : spd_level = spdlog::level::warn; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_ERROR   : spd_level = spdlog::level::err; break;
    case ReaderCGNS::Logger::READER_CGNS_LOG_CRITICAL: spd_level = spdlog::level::critical; break;
    default                                          : spd_level = spdlog::level::info; break;
    }

#ifndef NDEBUG
    std::string_view source = file != nullptr ? std::string_view(file) : std::string_view("unknown");
    if (const auto separator = source.find_last_of("/\\"); separator != std::string_view::npos) {
        source.remove_prefix(separator + 1);
    }
    logger->log(spd_level, "[ReaderCGNS] [{}:{}] {}", source, line, message != nullptr ? message : "EmptyMsg");
#else
    logger->log(spd_level, "[ReaderCGNS] {}", message != nullptr ? message : "EmptyMsg");
#endif
}

int main(int argc, char* argv[])
{
    if (!SINGLE_DATA.ProcessArguments(argc, argv)) {
        return EXIT_FAILURE;
    }
    SCOPED_TIMER_LOG("Main");

    if (!std::filesystem::exists(INPUT_PATH)) {
        LOG_ERROR("Input path does not exist");
        return -1;
    }

    const auto reader_cgns_logger = LOG;
    if (!ReaderCGNS::Logger::SetLogCallback(ReaderCGNSLogCallback, reader_cgns_logger.get())) {
        LOG_ERROR("Failed to install the ReaderCGNS log callback.");
        return EXIT_FAILURE;
    }
    const ReaderCGNSLogGuard reader_cgns_log_guard;
    std::jthread test_thread([&] { ReaderCGNS::info(INPUT_PATH); });

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

    test_thread.join();
    return 0;
}
