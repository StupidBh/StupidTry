#include "HDF5.hpp"

#include "log/logger.hpp"

template<class T, std::size_t N>
requires std::integral<T> || std::floating_point<T>
class CompTypeMatrix final {
public:
    static_assert(N > 0, "The matrix dimension must be greater than 0.");
    std::array<T, N> data;

    template<typename Container>
    requires requires(Container c) {
        { *std::begin(c) } -> std::convertible_to<std::string>;
    }
    static H5::CompType CompType(const Container& names)
    {
        constexpr size_t base_offset = offsetof(CompTypeMatrix, data);

        H5::CompType type(sizeof(CompTypeMatrix));
        for (std::size_t i = 0; i < names.size() && i < N; ++i) {
            type.insertMember(names[i], base_offset + sizeof(T) * i, HDF5Utils::H5_NATIVE_TYPE<T>());
        }

        return type;
    }
};

struct Grid
{
    char doc[32] = { 0 };
    std::uint32_t id = 1;
    std::array<double, 3> grid = { 1.0, 2.0, 3.0 };

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

        H5::H5File file(H5_FILE, H5F_ACC_RDONLY);
        auto dataset = file.openDataSet("Try");

        try {
            hsize_t dims = 0;
            dataset.getSpace().getSimpleExtentDims(&dims);
            coordinates.resize(dims);
            dataset.read(coordinates.data(), Grid::CompType());
        }
        catch (const H5::Exception& e) {
            LOG_ERROR("Failed: {}", e.getDetailMsg());
            return 1;
        }
    }

    return 0;
}
