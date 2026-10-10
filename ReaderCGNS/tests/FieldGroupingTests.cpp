#include "ReaderAPI/ReaderApiBase.h"
#include "cgnslib.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

namespace {
    void Check(bool condition, std::string_view message)
    {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    void CheckCgns(int status)
    {
        if (status != CG_OK) {
            throw std::runtime_error(cg_get_error());
        }
    }

    class ReaderModule final {
    public:
        using Reader = std::unique_ptr<ReaderAPI::ReaderApiBase, ReaderAPI::DestroyReaderCGNSFunc>;

        explicit ReaderModule(const std::filesystem::path& path) :
            m_library(LoadLibraryW(path.c_str()))
        {
            Check(this->m_library != nullptr, "Cannot load ReaderCGNS.dll.");
        }

        ~ReaderModule() { FreeLibrary(this->m_library); }

        ReaderModule(const ReaderModule&) = delete;
        ReaderModule& operator=(const ReaderModule&) = delete;

        Reader CreateReader() const
        {
            const auto create = reinterpret_cast<ReaderAPI::CreateReaderCGNSFunc>(GetProcAddress(this->m_library, "CreateReaderCGNS"));
            const auto destroy = reinterpret_cast<ReaderAPI::DestroyReaderCGNSFunc>(GetProcAddress(this->m_library, "DestroyReaderCGNS"));
            Check(create != nullptr && destroy != nullptr, "ReaderCGNS factory exports are missing.");
            Reader reader(create(), destroy);
            Check(reader != nullptr, "Reader creation failed.");
            return reader;
        }

    private:
        HMODULE m_library;
    };

    class TemporaryDirectory final {
    public:
        TemporaryDirectory() :
            m_path(std::filesystem::temp_directory_path() / ("reader-field-tests-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())))
        {
            Check(std::filesystem::create_directory(this->m_path), "Cannot create the fixture directory.");
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(this->m_path, error);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        std::filesystem::path File(std::string_view name) const { return this->m_path / name; }

    private:
        std::filesystem::path m_path;
    };

    struct ZoneSpec
    {
        CG_GridLocation_t location;
        std::vector<std::string> fields;
    };

    void CreateFixture(const std::filesystem::path& path, std::span<const ZoneSpec> zones)
    {
        int file = 0;
        CheckCgns(cg_open(path.string().c_str(), CG_MODE_WRITE, &file));
        try {
            int base = 0;
            CheckCgns(cg_base_write(file, "Base", 1, 3, &base));
            for (std::size_t i = 0; i < zones.size(); ++i) {
                int zone = 0;
                const std::array<cgsize_t, 3> size { 3, 2, 0 };
                CheckCgns(cg_zone_write(file, base, ("Zone" + std::to_string(i)).c_str(), size.data(), CG_Structured, &zone));
                int solution = 0;
                CheckCgns(cg_sol_write(file, base, zone, "Solution", zones[i].location, &solution));
                for (const auto& name : zones[i].fields) {
                    int field = 0;
                    const float start = static_cast<float>(100 * i + 1);
                    const std::array<float, 3> values { start, start + 1, start + 2 };
                    CheckCgns(cg_field_write(file, base, zone, solution, CG_RealSingle, name.c_str(), values.data(), &field));
                }
            }
        }
        catch (...) {
            cg_close(file);
            throw;
        }
        CheckCgns(cg_close(file));
    }

    struct ExpectedGroup
    {
        std::string var;
        int position;
        std::vector<std::string> sub_vars;
        std::vector<ReaderAPI::Integer> ids;
        std::vector<ReaderAPI::Real> data;
    };

    void CheckRejectedQuery(ReaderAPI::ReaderApiBase& reader, const std::string& var, const std::string& sub_var)
    {
        const std::vector<ReaderAPI::Integer> sentinel_ids { -42 };
        const std::vector<ReaderAPI::Real> sentinel_data { -42 };
        auto ids = sentinel_ids;
        auto data = sentinel_data;
        Check(reader.GetFieldFunctionPosition(var, sub_var) == -1, "Invalid group/subfield position lookup was accepted.");
        Check(!reader.GetFieldFunctionIds(var, sub_var, ids), "Invalid group/subfield ID lookup was accepted.");
        Check(!reader.GetFieldFunctionData(var, sub_var, data), "Invalid group/subfield data lookup was accepted.");
        Check(ids == sentinel_ids && data == sentinel_data, "Rejected field lookup modified caller outputs.");
    }

    void CheckGroups(ReaderAPI::ReaderApiBase& reader, std::span<const ExpectedGroup> expected)
    {
        for (int query = 0; query < 2; ++query) {
            std::vector<ReaderAPI::Field> fields { { "stale", { "stale" } } };
            Check(reader.GetAllFieldFunctionName(fields), "Field enumeration failed.");
            Check(fields.size() == expected.size(), "Unexpected field group count.");
            Check(std::ranges::is_sorted(fields, { }, &ReaderAPI::Field::var), "Field groups must have deterministic name order.");
            for (const auto& group : expected) {
                const auto field = std::ranges::find(fields, group.var, &ReaderAPI::Field::var);
                Check(field != fields.end(), "Expected field group is missing.");
                Check(field->sub_vars == group.sub_vars, "Field components were grouped or named incorrectly.");
                for (const auto& sub_var : field->sub_vars) {
                    Check(reader.GetFieldFunctionPosition(field->var, sub_var) == group.position, "A field group contains a different position.");
                    std::vector<ReaderAPI::Integer> ids;
                    std::vector<ReaderAPI::Real> data;
                    Check(reader.GetFieldFunctionIds(field->var, sub_var, ids), "Grouped field ID lookup failed.");
                    Check(reader.GetFieldFunctionData(field->var, sub_var, data), "Grouped field data lookup failed.");
                    const auto expected_ids = !group.ids.empty()
                                                  ? group.ids
                                                  : (group.position == 0 ? std::vector<ReaderAPI::Integer> { 0, 1, 2 } : std::vector<ReaderAPI::Integer> { 2, 3 });
                    const auto expected_data = !group.data.empty()
                                                   ? group.data
                                                   : (group.position == 0 ? std::vector<ReaderAPI::Real> { 1, 2, 3 } : std::vector<ReaderAPI::Real> { 101, 102 });
                    Check(ids == expected_ids && data == expected_data, "Grouped field lookup returned incorrect IDs or values.");
                    CheckRejectedQuery(reader, "missing-group", sub_var);
                    CheckRejectedQuery(reader, field->var, "missing-subfield");
                }
            }
        }
    }

    void CheckFixture(ReaderAPI::ReaderApiBase& reader,
                      const TemporaryDirectory& directory,
                      std::string_view name,
                      std::span<const ZoneSpec> zones,
                      std::span<const ExpectedGroup> expected)
    {
        const auto path = directory.File(name);
        CreateFixture(path, zones);
        Check(reader.Open(path.string()), "Cannot open the field grouping fixture.");
        CheckGroups(reader, expected);
    }

    void CheckFieldGrouping(ReaderAPI::ReaderApiBase& reader, const TemporaryDirectory& directory)
    {
        const std::array scalar_zones { ZoneSpec { CG_Vertex, { "Pressure" } }, ZoneSpec { CG_CellCenter, { "Pressure" } } };
        const std::array scalar_groups { ExpectedGroup { "Pressure_vertex", 0, { "Pressure" } }, ExpectedGroup { "Pressure_cellcenter", 1, { "Pressure" } } };
        CheckFixture(reader, directory, "scalars.cgns", scalar_zones, scalar_groups);

        const std::vector<std::string> components { "Velocity_0", "Velocity_1", "VelocityMagnitude" };
        const std::array mixed_zones { ZoneSpec { CG_Vertex, components }, ZoneSpec { CG_CellCenter, components } };
        const std::array mixed_groups { ExpectedGroup { "Velocity_vertex", 0, { "VelocityMagnitude", "Velocity_0", "Velocity_1" } },
                                        ExpectedGroup { "Velocity_cellcenter", 1, { "VelocityMagnitude", "Velocity_0", "Velocity_1" } } };
        CheckFixture(reader, directory, "mixed-components.cgns", mixed_zones, mixed_groups);

        const std::array split_zones { ZoneSpec { CG_Vertex, { "Velocity_0", "VelocityMagnitude" } }, ZoneSpec { CG_CellCenter, { "Velocity_1" } } };
        const std::array split_groups { ExpectedGroup { "Velocity_vertex", 0, { "VelocityMagnitude", "Velocity_0" } },
                                        ExpectedGroup { "Velocity_cellcenter", 1, { "Velocity_1" } } };
        CheckFixture(reader, directory, "split-components.cgns", split_zones, split_groups);
        CheckRejectedQuery(reader, "Velocity_vertex", "Velocity_1");
        CheckRejectedQuery(reader, "Velocity_cellcenter", "Velocity_0");

        const std::array partial_zones { ZoneSpec { CG_Vertex, { "Velocity_0", "Velocity_1" } }, ZoneSpec { CG_CellCenter, { "Velocity_0" } } };
        const std::array partial_groups { ExpectedGroup { "Velocity_vertex", 0, { "Velocity_0", "Velocity_1" } },
                                          ExpectedGroup { "Velocity_cellcenter", 1, { "Velocity_0" } } };
        CheckFixture(reader, directory, "partial-overlap.cgns", partial_zones, partial_groups);
        CheckRejectedQuery(reader, "Velocity_cellcenter", "Velocity_1");

        const std::array vertex_zones { ZoneSpec { CG_Vertex, components } };
        const std::array vertex_groups { ExpectedGroup { "Velocity", 0, { "VelocityMagnitude", "Velocity_0", "Velocity_1" } } };
        CheckFixture(reader, directory, "vertex-only.cgns", vertex_zones, vertex_groups);

        const std::array cell_zones { ZoneSpec { CG_Vertex, { } }, ZoneSpec { CG_CellCenter, components } };
        const std::array cell_groups { ExpectedGroup { "Velocity", 1, { "VelocityMagnitude", "Velocity_0", "Velocity_1" } } };
        CheckFixture(reader, directory, "cell-only.cgns", cell_zones, cell_groups);

        const std::array literal_zones { ZoneSpec { CG_Vertex, { "Stress_Vertex" } } };
        const std::array literal_groups { ExpectedGroup { "Stress", 0, { "Stress_Vertex" } } };
        CheckFixture(reader, directory, "literal-suffix.cgns", literal_zones, literal_groups);

        const std::vector<std::string> u_components { "u_1", "u_2" };
        const std::array u_zones { ZoneSpec { CG_Vertex, u_components }, ZoneSpec { CG_CellCenter, u_components } };
        const std::array u_groups { ExpectedGroup { "u_vertex", 0, u_components }, ExpectedGroup { "u_cellcenter", 1, u_components } };
        CheckFixture(reader, directory, "u-components.cgns", u_zones, u_groups);
        CheckRejectedQuery(reader, "u", "u_1");
        CheckRejectedQuery(reader, "u_Vertex", "u_1");
        CheckRejectedQuery(reader, "u_vertex", "u_1_Vertex");

        const std::array distinct_zones { ZoneSpec { CG_Vertex, { "Pressure", "Density" } } };
        const std::array distinct_groups { ExpectedGroup { "Pressure", 0, { "Pressure" } }, ExpectedGroup { "Density", 0, { "Density" } } };
        CheckFixture(reader, directory, "distinct-groups.cgns", distinct_zones, distinct_groups);
        CheckRejectedQuery(reader, "Pressure", "Density");
        CheckRejectedQuery(reader, "Density", "Pressure");

        const std::array aggregate_zones { ZoneSpec { CG_Vertex, u_components }, ZoneSpec { CG_Vertex, u_components } };
        const std::array aggregate_groups { ExpectedGroup { "u", 0, u_components, { 0, 1, 2, 3, 4, 5 }, { 1, 2, 3, 101, 102, 103 } } };
        CheckFixture(reader, directory, "aggregate-zones.cgns", aggregate_zones, aggregate_groups);

        const std::array empty_zones { ZoneSpec { CG_Vertex, { } } };
        const auto empty_path = directory.File("empty-fields.cgns");
        CreateFixture(empty_path, empty_zones);
        Check(reader.Open(empty_path.string()), "Cannot open the empty-field fixture.");
        for (int query = 0; query < 2; ++query) {
            std::vector<ReaderAPI::Field> fields { { "sentinel", { "sentinel" } } };
            Check(!reader.GetAllFieldFunctionName(fields), "Empty field enumeration must return false.");
            Check(fields.size() == 1 && fields.front().var == "sentinel" && fields.front().sub_vars == std::vector<std::string> { "sentinel" },
                  "Empty field enumeration modified caller output.");
        }

        const std::array collision_zones { ZoneSpec { CG_Vertex, { "u", "u_vertex_1" } }, ZoneSpec { CG_CellCenter, { "u" } } };
        const auto collision_path = directory.File("group-collision.cgns");
        CreateFixture(collision_path, collision_zones);
        Check(reader.Open(collision_path.string()), "Cannot open the group collision fixture.");
        for (int query = 0; query < 2; ++query) {
            std::vector<ReaderAPI::Field> fields { { "sentinel", { "sentinel" } } };
            Check(!reader.GetAllFieldFunctionName(fields), "Ambiguous public group names were accepted.");
            Check(fields.size() == 1 && fields.front().var == "sentinel", "Failed layout initialization modified caller output.");
            CheckRejectedQuery(reader, "u_vertex", "u");
        }
        CheckFixture(reader, directory, "after-failure.cgns", u_zones, u_groups);

        reader.Close();
        std::vector<ReaderAPI::Field> fields { { "sentinel", { "sentinel" } } };
        Check(!reader.GetAllFieldFunctionName(fields), "Closed readers must reject field enumeration.");
        Check(fields.size() == 1 && fields.front().var == "sentinel", "Failed enumeration modified caller output.");
    }
} // namespace

int main(int argc, char* argv[])
{
    try {
        Check(argc == 2, "Expected ReaderCGNS.dll path.");
        const ReaderModule module { std::filesystem::path(argv[1]) };
        const TemporaryDirectory directory;
        auto reader = module.CreateReader();
        CheckFieldGrouping(*reader, directory);
        std::cout << "Field grouping checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Field grouping test failed: " << error.what() << '\n';
        return 1;
    }
}
