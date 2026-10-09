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
            this->m_create = reinterpret_cast<ReaderAPI::CreateReaderCGNSFunc>(GetProcAddress(this->m_library, "CreateReaderCGNS"));
            this->m_destroy = reinterpret_cast<ReaderAPI::DestroyReaderCGNSFunc>(GetProcAddress(this->m_library, "DestroyReaderCGNS"));
            if (this->m_create == nullptr || this->m_destroy == nullptr) {
                FreeLibrary(this->m_library);
                throw std::runtime_error("ReaderCGNS factory exports are missing.");
            }
        }

        ~ReaderModule() { FreeLibrary(this->m_library); }

        ReaderModule(const ReaderModule&) = delete;
        ReaderModule& operator=(const ReaderModule&) = delete;

        Reader CreateReader() const
        {
            Reader reader(this->m_create(), this->m_destroy);
            Check(reader != nullptr, "Reader creation failed.");
            return reader;
        }

    private:
        HMODULE m_library;
        ReaderAPI::CreateReaderCGNSFunc m_create = nullptr;
        ReaderAPI::DestroyReaderCGNSFunc m_destroy = nullptr;
    };

    class TemporaryDirectory final {
    public:
        TemporaryDirectory() :
            m_path(std::filesystem::temp_directory_path() / ("reader-fluid-tests-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())))
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

    class CgnsFile final {
    public:
        explicit CgnsFile(const std::filesystem::path& path) { CheckCgns(cg_open(path.string().c_str(), CG_MODE_WRITE, &this->m_id)); }

        ~CgnsFile()
        {
            if (this->m_id != 0) {
                cg_close(this->m_id);
            }
        }

        CgnsFile(const CgnsFile&) = delete;
        CgnsFile& operator=(const CgnsFile&) = delete;

        int Id() const { return this->m_id; }

        void Close()
        {
            CheckCgns(cg_close(this->m_id));
            this->m_id = 0;
        }

    private:
        int m_id = 0;
    };

    struct ZoneSpec
    {
        CG_GridLocation_t location;
        unsigned components;
    };

    std::string CreateFixture(const TemporaryDirectory& directory,
                              std::string_view name,
                              std::span<const ZoneSpec> zones,
                              const std::array<const char*, 3>& component_names = { "VelocityX", "VelocityY", "VelocityZ" })
    {
        const auto path = directory.File(name);
        CgnsFile file(path);
        int base = 0;
        CheckCgns(cg_base_write(file.Id(), "Base", 1, 3, &base));
        for (std::size_t i = 0; i < zones.size(); ++i) {
            int zone = 0;
            const std::array<cgsize_t, 3> size { 3, 2, 0 };
            CheckCgns(cg_zone_write(file.Id(), base, ("Zone" + std::to_string(i)).c_str(), size.data(), CG_Structured, &zone));
            int coordinate = 0;
            const std::array<float, 3> coordinates { 0, 1, 2 };
            CheckCgns(cg_coord_write(file.Id(), base, zone, CG_RealSingle, "CoordinateX", coordinates.data(), &coordinate));
            if (zones[i].components == 0) {
                continue;
            }
            int solution = 0;
            CheckCgns(cg_sol_write(file.Id(), base, zone, "Solution", zones[i].location, &solution));
            for (std::size_t component = 0; component < component_names.size(); ++component) {
                if ((zones[i].components & (1U << component)) == 0) {
                    continue;
                }
                const float start = static_cast<float>(100 * i + 10 * component + 1);
                const std::array<float, 3> values { start, start + 1, start + 2 };
                int field = 0;
                CheckCgns(cg_field_write(file.Id(), base, zone, solution, CG_RealSingle, component_names[component], values.data(), &field));
            }
        }
        file.Close();
        return path.string();
    }

    void CheckMetadata(const ReaderModule& module, const TemporaryDirectory& directory)
    {
        const std::array vertex_zones { ZoneSpec { CG_Vertex, 7 } };
        const std::array cell_zones { ZoneSpec { CG_CellCenter, 7 } };
        const std::array missing_zones { ZoneSpec { CG_Vertex, 3 } };
        const std::array empty_zones { ZoneSpec { CG_Vertex, 0 } };
        const auto vertex = CreateFixture(directory, "vertex.cgns", vertex_zones);
        const auto cell = CreateFixture(directory, "cell.cgns", cell_zones);
        const auto missing = CreateFixture(directory, "missing.cgns", missing_zones);
        const auto empty = CreateFixture(directory, "empty.cgns", empty_zones);

        auto reader = module.CreateReader();
        auto* extension = reader->GetFluidExtensions();
        Check(extension != nullptr, "ReaderCGNS must expose its fluid extension before Open().");
        Check(!extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "Closed readers must not expose velocity metadata.");
        Check(reader->Open(vertex), "Cannot open the vertex fixture.");
        Check(extension->GetVelocityFieldPosition() == 0, "Position queries must work without a preceding HasVelocityField().");
        for (int query = 0; query < 3; ++query) {
            Check(extension->HasVelocityField() && extension->GetVelocityFieldPosition() == 0, "Repeated metadata queries changed the result.");
        }
        Check(reader->Open(missing), "Cannot open the missing-component fixture.");
        Check(reader->GetFluidExtensions() == extension, "Open() changed the borrowed extension address.");
        Check(!extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "File switches retained stale velocity components.");
        Check(reader->Open(cell), "Cannot open the cell fixture.");
        Check(extension->GetVelocityFieldPosition() == 1 && extension->HasVelocityField(), "Cell-centered velocity metadata was not detected.");
        reader->Close();
        Check(!extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "Close() retained velocity metadata.");
        Check(reader->Open(empty), "Cannot open the empty-field fixture.");
        Check(!extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "Empty fields must not provide velocity metadata.");
    }

    void CheckData(const ReaderModule& module, const TemporaryDirectory& directory)
    {
        const std::array vertex_zones { ZoneSpec { CG_Vertex, 7 } };
        const std::array cell_zones { ZoneSpec { CG_CellCenter, 7 } };
        const std::array sparse_zones { ZoneSpec { CG_Vertex, 7 }, ZoneSpec { CG_Vertex, 0 }, ZoneSpec { CG_Vertex, 7 } };
        const std::array missing_zones { ZoneSpec { CG_Vertex, 3 } };
        const std::array position_zones { ZoneSpec { CG_Vertex, 1 }, ZoneSpec { CG_CellCenter, 6 } };
        const std::array id_zones { ZoneSpec { CG_Vertex, 3 }, ZoneSpec { CG_Vertex, 4 } };
        const auto vertex = CreateFixture(directory, "vertex.cgns", vertex_zones);
        const auto cell = CreateFixture(directory, "cell.cgns", cell_zones);
        const auto sparse = CreateFixture(directory, "sparse.cgns", sparse_zones);
        const auto missing = CreateFixture(directory, "missing.cgns", missing_zones);
        const auto positions = CreateFixture(directory, "positions.cgns", position_zones);
        const auto ids_mismatch = CreateFixture(directory, "ids.cgns", id_zones);

        auto reader = module.CreateReader();
        auto* extension = reader->GetFluidExtensions();
        const std::vector<std::vector<ReaderAPI::Real>> sentinel_data { { -42 } };
        const std::vector<ReaderAPI::Integer> sentinel_ids { -42 };
        auto data = sentinel_data;
        auto ids = sentinel_ids;
        const auto check_failure = [&] {
            data = sentinel_data;
            ids = sentinel_ids;
            Check(!extension->GetVelocityField(data, ids), "Invalid velocity data was accepted.");
            Check(data == sentinel_data && ids == sentinel_ids, "Failed velocity reads modified caller outputs.");
        };
        check_failure();
        Check(reader->Open(vertex), "Cannot open the vertex fixture.");
        const std::vector<std::vector<ReaderAPI::Real>> vertex_data { { 1, 2, 3 }, { 11, 12, 13 }, { 21, 22, 23 } };
        const std::vector<ReaderAPI::Integer> vertex_ids { 0, 1, 2 };
        for (int query = 0; query < 3; ++query) {
            Check(extension->GetVelocityField(data, ids), "Vertex velocity read failed.");
            Check(data == vertex_data && ids == vertex_ids, "Velocity values must replace outputs in X/Y/Z order.");
        }
        Check(reader->Open(cell), "Cannot open the cell fixture.");
        const std::vector<std::vector<ReaderAPI::Real>> cell_data { { 1, 2 }, { 11, 12 }, { 21, 22 } };
        const std::vector<ReaderAPI::Integer> cell_ids { 0, 1 };
        Check(extension->GetVelocityField(data, ids) && data == cell_data && ids == cell_ids, "Cell-centered velocity values are incorrect.");
        Check(reader->Open(sparse), "Cannot open the sparse fixture.");
        const std::vector<std::vector<ReaderAPI::Real>> sparse_data { { 1, 2, 3, 201, 202, 203 }, { 11, 12, 13, 211, 212, 213 }, { 21, 22, 23, 221, 222, 223 } };
        const std::vector<ReaderAPI::Integer> sparse_ids { 0, 1, 2, 6, 7, 8 };
        Check(extension->GetVelocityField(data, ids) && data == sparse_data && ids == sparse_ids, "Aligned sparse IDs must be preserved across zones.");
        Check(reader->Open(missing), "Cannot open the missing-component fixture.");
        check_failure();
        Check(reader->Open(positions), "Cannot open the position-mismatch fixture.");
        Check(extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "Mixed component locations must be rejected.");
        check_failure();
        Check(reader->Open(ids_mismatch), "Cannot open the ID-mismatch fixture.");
        Check(extension->HasVelocityField() && extension->GetVelocityFieldPosition() == 0, "ID mismatches should not affect name/position checks.");
        check_failure();
        reader->Close();
        check_failure();
    }

    void CheckAliases(const ReaderModule& module, const TemporaryDirectory& directory)
    {
        constexpr std::array<std::array<const char*, 3>, 6> ALIAS_SETS { {
            { "vELoCiTyX", "VELOCITYY", "velocityz" },
            { "Velocity_0", "VELOCITY_1", "velocity_2" },
            { "Velocity[I]", "VELOCITY[j]", "velocity[K]" },
            { "UX", "uY", "Uz" },
            { "Velocity_VectorsX", "VELOCITY_VECTORSY", "velocity_vectorsZ" },
            { "UX", "Velocity_1", "velocity[K]" },
        } };
        const std::array zones { ZoneSpec { CG_Vertex, 7 } };
        const std::vector<std::vector<ReaderAPI::Real>> expected_data { { 1, 2, 3 }, { 11, 12, 13 }, { 21, 22, 23 } };
        const std::vector<ReaderAPI::Integer> expected_ids { 0, 1, 2 };
        auto reader = module.CreateReader();
        auto* extension = reader->GetFluidExtensions();
        std::vector<std::vector<ReaderAPI::Real>> data;
        std::vector<ReaderAPI::Integer> ids;
        for (std::size_t i = 0; i < ALIAS_SETS.size(); ++i) {
            const auto fixture = CreateFixture(directory, "alias-" + std::to_string(i) + ".cgns", zones, ALIAS_SETS[i]);
            Check(reader->Open(fixture), "Cannot open the alias fixture.");
            Check(extension->GetVelocityField(data, ids) && data == expected_data && ids == expected_ids,
                  "Alias reads must use the original field names and preserve X/Y/Z order.");
            Check(extension->HasVelocityField() && extension->GetVelocityFieldPosition() == 0, "Mixed-case velocity aliases were not detected.");
            Check(extension->GetVelocityField(data, ids) && data == expected_data && ids == expected_ids, "Repeated alias reads changed the result.");
        }

        constexpr std::array<std::array<const char*, 3>, 5> INVALID_NAMES { {
            { "VelocityXExtra", "VelocityY", "VelocityZ" },
            { "myVelocityX", "VelocityY", "VelocityZ" },
            { "VelocityX_Vertex", "VelocityY_Vertex", "VelocityZ_Vertex" },
            { "UX", "UY", "NotVelocityZ" },
            { "VelocityX", "UX", "Velocity_0" },
        } };
        for (std::size_t i = 0; i < INVALID_NAMES.size(); ++i) {
            const auto fixture = CreateFixture(directory, "invalid-alias-" + std::to_string(i) + ".cgns", zones, INVALID_NAMES[i]);
            Check(reader->Open(fixture), "Cannot open the invalid-alias fixture.");
            Check(!extension->HasVelocityField() && extension->GetVelocityFieldPosition() == -1, "Velocity aliases must match the entire normalized name.");
            data = expected_data;
            ids = expected_ids;
            Check(!extension->GetVelocityField(data, ids) && data == expected_data && ids == expected_ids, "Missing aliases modified caller outputs.");
        }
    }

    struct LogEntry
    {
        ReaderAPI::Logger::LogLevel level;
        std::string file;
        int line;
        std::string message;
    };

    void CaptureLog(void* context, ReaderAPI::Logger::LogLevel level, const char* file, int line, const char* message)
    {
        static_cast<std::vector<LogEntry>*>(context)->push_back(LogEntry { level, file, line, message });
    }

    void CheckLogging(const ReaderModule& module, const TemporaryDirectory& directory)
    {
        const std::array position_zones { ZoneSpec { CG_Vertex, 1 }, ZoneSpec { CG_CellCenter, 6 } };
        const auto fixture = CreateFixture(directory, "positions.cgns", position_zones);
        std::vector<LogEntry> first_logs;
        std::vector<LogEntry> second_logs;
        std::vector<LogEntry> rebound_logs;
        const auto count_errors = [](const std::vector<LogEntry>& entries) {
            return std::ranges::count_if(entries, [](const LogEntry& entry) { return entry.level == ReaderAPI::Logger::READER_CGNS_LOG_ERROR; });
        };
        auto first = module.CreateReader();
        auto second = module.CreateReader();
        Check(first->SetLogCallback(CaptureLog, &first_logs) && second->SetLogCallback(CaptureLog, &second_logs), "Cannot register reader callbacks.");
        Check(first->Open(fixture) && second->Open(fixture), "Cannot open logging fixtures.");
        first_logs.clear();
        second_logs.clear();
        auto* extension = first->GetFluidExtensions();
        Check(extension->GetVelocityFieldPosition() == -1, "Position mismatch was not rejected.");
        Check(count_errors(first_logs) == 1 && second_logs.empty(), "Extension logs must use only the owning reader callback.");
        const auto& entry = *std::ranges::find_if(first_logs, [](const LogEntry& log) { return log.level == ReaderAPI::Logger::READER_CGNS_LOG_ERROR; });
        Check(entry.level == ReaderAPI::Logger::READER_CGNS_LOG_ERROR && entry.file.ends_with("FluidExtensions.cpp") && entry.line > 0 &&
                  entry.message.find("VelocityY") != std::string::npos && entry.message.find("position 1, expected 0") != std::string::npos,
              "Extension logs lost their severity, source location, or component positions.");
        Check(first->ClearLogCallback(), "Cannot clear the owning reader callback.");
        first_logs.clear();
        Check(extension->GetVelocityFieldPosition() == -1 && first_logs.empty() && second_logs.empty(), "Cleared callbacks still received extension logs.");
        Check(first->SetLogCallback(CaptureLog, &rebound_logs), "Cannot rebind the owning reader callback.");
        Check(extension->GetVelocityFieldPosition() == -1 && count_errors(rebound_logs) == 1 && first_logs.empty() && second_logs.empty(),
              "Extension logs did not follow callback rebinding.");
        const auto rebound_count = rebound_logs.size();
        Check(second->GetFluidExtensions()->GetVelocityFieldPosition() == -1 && count_errors(second_logs) == 1 && rebound_logs.size() == rebound_count,
              "Rebinding one reader affected another reader's logging.");
        Check(first->ClearLogCallback() && second->ClearLogCallback(), "Cannot clear test callbacks.");
    }
} // namespace

int main(int argc, char* argv[])
{
    try {
        Check(argc == 3, "Expected ReaderCGNS.dll path and a test mode.");
        const ReaderModule module { std::filesystem::path(argv[1]) };
        const TemporaryDirectory directory;
        const std::string_view mode(argv[2]);
        if (mode == "metadata") {
            CheckMetadata(module, directory);
        }
        else if (mode == "data") {
            CheckData(module, directory);
        }
        else if (mode == "aliases") {
            CheckAliases(module, directory);
        }
        else if (mode == "logging") {
            CheckLogging(module, directory);
        }
        else {
            throw std::runtime_error("Unknown fluid extension test mode.");
        }
        std::cout << "Fluid extension " << mode << " checks passed.\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Fluid extension test failed: " << error.what() << '\n';
        return 1;
    }
}
