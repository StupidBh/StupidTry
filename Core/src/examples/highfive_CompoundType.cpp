#include "HDF5.hpp"
#include "HighFive.hpp"
#include "log/logger.hpp"

struct Grid
{
    char doc[32] = { 0 };
    std::uint32_t id = 1;
    std::array<double, 3> grid = { 1.0, 2.0, 3.0 };

    static HighFive::CompoundType CompType()
    {
        static constinit hsize_t array_dims[1] = { 3 };
        static H5::ArrayType array_type(HDF5Utils::H5_NATIVE_TYPE<decltype(grid)::value_type>(), 1, array_dims);

        static HighFive::CompoundType result(
            std::vector<HighFive::CompoundType::member_def> {
                { "ID", HighFive::create_datatype<std::uint32_t>(), offsetof(Grid, id) }, //
                { "GRID", HighFive::DataType(array_type.getId()), offsetof(Grid, grid) }, //
                { "DOC",
                  HighFive::FixedLengthStringType(31, HighFive::StringPadding::NullTerminated),
                  offsetof(Grid, doc) }, //

            },
            sizeof(Grid) //
        );

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
        return fmt::format_to(ctx.out(), "(ID={}, DOC={}, GRID={})", p.id, p.doc, p.grid);
    }
};

int Test()
{
    std::vector<Grid> coordinates = {
        Grid { .doc = "Test1", .id = 0 }, //
        Grid { .doc = "Test2", .id = 1 }, //
        Grid { .doc = "Test3", .id = 2 }, //
        Grid { .doc = "Test4", .id = 3 }, //
        Grid { .doc = "Test5", .id = 4 }, //
    };

    const std::string H5_FILE = "./Try.h5";

    {
        HighFive::File file(H5_FILE, HighFive::File::Overwrite);

        try {
            HFUtils::WriteDataSet(file, "Try", coordinates, Grid::CompType());
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    {
        coordinates.clear();
        HighFive::File file(H5_FILE, HighFive::File::ReadOnly);

        auto dataset = file.getDataSet("Try");
        auto dataset_dims = dataset.getDimensions();
        coordinates.resize(dataset_dims.front());
        try {
            dataset.read_raw(coordinates.data(), Grid::CompType());
        }
        catch (const HighFive::Exception& e) {
            LOG_ERROR("Failed: {}", e.what());
            return 1;
        }
    }

    return 0;
}
