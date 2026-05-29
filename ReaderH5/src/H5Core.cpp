#include "H5Core.h"

#include "HDF5.hpp"
#include "HighFive.hpp"
#include "ScopedTimer.hpp"

#define SCOPED_TIMER_LOG(out_msg)                \
    decltype(auto) CONCAT(timer_, __COUNTER__) = \
        utils::ScopedTimer(std::string_view(out_msg), [](std::string_view msg) { LOG_INFO(msg); })

void stupid_h5::InitLog(std::shared_ptr<spdlog::logger> log)
{
    dylog::Logger::get_instance().UpdateLog(log);
}

struct Grid
{
    char doc[32] = { 0 };
    std::uint32_t id = 1;
    std::array<double, 3> grid = { 1.0, 2.0, 3.0 };

    static HighFive::CompoundType CompoundType()
    {
        static constinit hsize_t array_dims[1] = { 3 };
        H5::ArrayType array_type(HDF5Utils::H5_NATIVE_TYPE<decltype(grid)::value_type>(), 1, array_dims);

        HighFive::CompoundType result(
            std::vector<HighFive::CompoundType::member_def> {
                { "ID", HighFive::create_datatype<std::uint32_t>(), offsetof(Grid, id) }, //
                { "GRID", HighFive::DataType(array_type.getId()), offsetof(Grid, grid) }, //
                { "DOC",
                  HighFive::FixedLengthStringType(31, HighFive::StringPadding::NullTerminated),
                  offsetof(Grid, doc) },

            },
            sizeof(Grid) //
        );

        return result;
    }

    static H5::CompType CompType()
    {
        static constinit hsize_t array_dims[1] = { 3 };
        static H5::ArrayType array_type(HDF5Utils::H5_NATIVE_TYPE<decltype(grid)::value_type>(), 1, array_dims);
        static H5::StrType str_type(H5::PredType::C_S1, 31);

        H5::CompType result(sizeof(Grid));
        result.insertMember("ID", offsetof(Grid, id), HDF5Utils::H5_NATIVE_TYPE<decltype(id)>());
        result.insertMember("GRID", offsetof(Grid, grid), array_type);
        result.insertMember("DOC", offsetof(Grid, doc), str_type);

        return result;
    }
};

template<>
struct fmt::formatter<Grid>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template<class FormatContext>
    auto format(const Grid& p, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "\n(ID={}, DOC=\"{}\", GRID={})", p.id, p.doc, p.grid);
    }
};

int stupid_h5::TestHighFive()
{
    std::vector<Grid> coordinates = {
        Grid { .doc = "Test0", .id = 0 }, //
        Grid { .doc = "Test1", .id = 1 }, //
        Grid { .doc = "Test2", .id = 2 }, //
        Grid { .doc = "Test3", .id = 3 }, //
        Grid { .doc = "Test4", .id = 4 }, //
    };
    coordinates.resize(100);

    const std::string HF_FILE = "./Try1.h5";

    {
        SCOPED_TIMER_LOG(std::format("HighFive::Wirte file: [{}]", HF_FILE));
        HighFive::File file(HF_FILE, HighFive::File::Overwrite);

        try {
            HFUtils::WriteDataSet(file, "Try", coordinates, Grid::CompoundType());
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    {
        coordinates.clear();
        SCOPED_TIMER_LOG("HighFive::Read file");

        HighFive::File file(HF_FILE, HighFive::File::ReadOnly);
        HighFive::DataSet dataset = file.getDataSet("Try");
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

    return 0;
}

int stupid_h5::TestH5()
{
    std::vector<Grid> coordinates = {
        Grid { .doc = "Test1", .id = 0 }, //
        Grid { .doc = "Test2", .id = 1 }, //
        Grid { .doc = "Test3", .id = 2 }, //
        Grid { .doc = "Test4", .id = 3 }, //
        Grid { .doc = "Test5", .id = 4 }, //
    };
    coordinates.resize(100);

    const std::string H5_FILE = "./Try2.h5";

    {
        SCOPED_TIMER_LOG(std::format("H5::Wirte file: [{}]", H5_FILE));
        H5::H5File file(H5_FILE, H5F_ACC_TRUNC);

        try {
            HDF5Utils::WriteDataSet(file, "Try", coordinates, Grid::CompType());
        }
        catch (const H5::Exception& e) {
            LOG_ERROR("Failed: {}", e.getDetailMsg());
            return 1;
        }
    }

    {
        coordinates.clear();
        SCOPED_TIMER_LOG("H5::Read file");

        H5::H5File file(H5_FILE, H5F_ACC_RDONLY);
        auto dataset = file.openDataSet("Try");
        hsize_t dims = 0;
        dataset.getSpace().getSimpleExtentDims(&dims);
        coordinates.resize(dims);

        try {
            dataset.read(coordinates.data(), Grid::CompType());
        }
        catch (const H5::Exception& e) {
            LOG_ERROR("Failed: {}", e.getDetailMsg());
            return 1;
        }
    }
    return 0;
}
