#include "Functions.h"
#include "SingletonData.h"

#include "ReaderCGNS/CgnsCore.h"
#include "HighFiveUtils.hpp"
#include "Utils/BlockingQueue.hpp"

void ReaderCGNSLogCallback(int level, const char* file, int line, const char* function, const char* message)
{
    spdlog::level::level_enum spd_level;
    switch (level) {
    case READER_CGNS_LOG_DEBUG: spd_level = spdlog::level::debug; break;
    case READER_CGNS_LOG_WARN : spd_level = spdlog::level::warn; break;
    case READER_CGNS_LOG_ERROR: spd_level = spdlog::level::err; break;
    default                   : spd_level = spdlog::level::info;
    }

#ifndef NDEBUG
    const std::filesystem::path file_path = file;
    LOG->log(spd_level, "[ReaderCGNS] [{}:{}:{}] {}", file_path.filename(), line, function, message != nullptr ? message : "");
#else
    LOG->log(spd_level, "[ReaderCGNS] {}", message != nullptr ? message : "");
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

    ReaderCGNS::Logger::SetLogCallback(ReaderCGNSLogCallback);
    std::jthread test_thread([&] { ReaderCGNS::info(INPUT_PATH); });

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

    std::vector<Grid> coordinates(1000);
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

    {
        SCOPED_TIMER_LOG("ProducerConsumer demo");
        constexpr std::size_t DATA_COUNT = 10000;

        utils::BlockingQueue<std::size_t> queue(10);
        std::vector<std::size_t> consumed;
        consumed.reserve(DATA_COUNT);

        LOG_INFO("ProducerConsumer demo start, count={}, queue capacity={}", DATA_COUNT, queue.Capacity());

        std::thread producer([&] {
            for (std::size_t i = 0; i < DATA_COUNT; ++i) {
                if (!queue.Push(i)) {
                    LOG_WARN("Producer stopped early because queue is closed, next value={}", i);
                    break;
                }
            }
            queue.Close();
            LOG_INFO("Producer finished, queue closed");
        });

        std::thread consumer([&] {
            while (auto value = queue.Pop()) {
                consumed.emplace_back(*value);
            }
            LOG_INFO("Consumer finished, consumed={}", consumed.size());
        });

        LOG_INFO("Producer and consumer threads launched in parallel");
        producer.join();
        consumer.join();

        LOG_INFO("ProducerConsumer demo finished, total consumed={}", consumed.size());
        const bool is_valid = consumed.size() == DATA_COUNT &&
                              std::accumulate(consumed.begin(), consumed.end(), std::size_t { 0 }) == DATA_COUNT * (DATA_COUNT - 1) / 2;

        if (!is_valid) {
            LOG_ERROR("ProducerConsumer demo failed, consumed size={}", consumed.size());
            return 1;
        }
        LOG_INFO("ProducerConsumer demo passed, consumed size={}", consumed.size());
    }

    test_thread.join();
    ReaderCGNS::Logger::ClearLogCallback();
    return 0;
}
