#include "FileManager.h"

#include "CgnsTypes.hpp"

FileManager::~FileManager()
{
    this->Close();
}

bool FileManager::SetLogCallback(const ReaderAPI::Logger::LogCallback callback, void* context) noexcept
{
    return this->m_log_dispatcher.SetCallback(callback, context);
}

bool FileManager::ClearLogCallback() noexcept
{
    return this->m_log_dispatcher.ClearCallback();
}

bool FileManager::Open(const std::string& cgns_file_path)
{
    if (this->IsOpen()) {
        if (this->m_cgns_file_path == cgns_file_path) {
            LOG_INFO("CGNS [{}] already open", this->m_cgns_file_path);
            return true;
        }
        this->Close();
    }
    LOG_INFO("Open in read only: [{}]", cgns_file_path);

    int cgns_file_type = -1;
    const int status = cg_is_cgns(cgns_file_path.c_str(), &cgns_file_type);
    auto FileTypeName = [](int file_type) -> const char* {
        switch (file_type) {
            case CG_FILE_ADF : return "ADF";
            case CG_FILE_ADF2: return "ADF2";
            case CG_FILE_HDF5: return "HDF5";
            default          : return "ERROR_FILE";
        }
    };
    if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
        LOG_INFO("[CG_ERROR] [{}] msg: {}", FileTypeName(cgns_file_type), cg_get_error());
        return false;
    }

    if (CGNS_LOG_CALL(cg_open(cgns_file_path.c_str(), CG_MODE_READ, &this->m_file_id)) != CG_OK) {
        return false;
    }

    float cg_file_version = 0.F;
    int cg_file_precision = 0;
    CGNS_LOG_CALL(cg_version(this->m_file_id, &cg_file_version));
    CGNS_LOG_CALL(cg_precision(this->m_file_id, &cg_file_precision));
    LOG_INFO("[{}] v{:.2f}, Precision={}", FileTypeName(cgns_file_type), cg_file_version, cg_file_precision);

    this->m_cgns_file_path = cgns_file_path;
    if (!this->initialize_base_zone_layout()) {
        this->Close();
        return false;
    }

    return true;
}

void FileManager::Close()
{
    if (this->IsOpen()) {
        LOG_INFO("Close CGNS file: [{}].", this->m_cgns_file_path);
        CGNS_LOG_CALL(cg_close(this->m_file_id));
    }
    this->clear_cache_data();
    this->clear_file_data();
}

bool FileManager::IsOpen() const
{
    return this->m_file_id != 0 && !this->m_cgns_file_path.empty();
}

float FileManager::GetVersion() const
{
    float cg_file_version = 0.F;
    CGNS_LOG_CALL(cg_version(this->m_file_id, &cg_file_version));
    return cg_file_version;
}

std::string FileManager::GetSolverType() const
{
    CG_GoverningEquationsType_t solver_type = CG_GoverningEquationsType_t::CG_GoverningEquationsNull;
    if (CGNS_LOG_CALL(cg_goto(this->m_file_id, 1, "FlowEquationSet_t", 1, "end")) != CG_OK || CGNS_LOG_CALL(cg_governing_read(&solver_type)) != CG_OK) {
        return "Unknown";
    }

    const char* solver_type_name = cg_GoverningEquationsTypeName(solver_type);
    return solver_type_name != nullptr ? solver_type_name : "Unknown";
}

void FileManager::clear_cache_data() noexcept
{
}

int FileManager::get_file_id() const noexcept
{
    return this->m_file_id;
}

std::vector<std::pair<int, std::vector<int>>> FileManager::get_base_zone_indices() const
{
    std::vector<std::pair<int, std::vector<int>>> base_zone_indices;
    base_zone_indices.reserve(this->m_base_zone_indices.size());
    for (const auto& [base_index, base_zone] : this->m_base_zone_indices) {
        base_zone_indices.emplace_back(base_index, base_zone.zone_indices);
    }
    return base_zone_indices;
}

const FileManager::BaseZone* FileManager::get_base_zone_indices(const int base) const noexcept
{
    const auto iter = this->m_base_zone_indices.find(base);
    return iter != this->m_base_zone_indices.end() ? &iter->second : nullptr;
}

const FileManager::BaseZone* FileManager::get_base_zone_indices(const std::string& base_name) const noexcept
{
    const auto iter = this->m_base_zone_layout.find(base_name);
    return iter != this->m_base_zone_layout.end() ? iter->second : nullptr;
}

LogDispatcher& FileManager::GetLogDispatcher() const noexcept
{
    return this->m_log_dispatcher;
}

void FileManager::clear_file_data() noexcept
{
    this->m_file_id = 0;
    utils::DeepClear(this->m_cgns_file_path, this->m_base_zone_indices, this->m_base_zone_layout);
}

bool FileManager::initialize_base_zone_layout()
{
    int nbases = 0;
    if (CGNS_LOG_CALL(cg_nbases(this->m_file_id, &nbases)) != CG_OK) {
        LOG_ERROR("No base data in CGNS.");
        return false;
    }

    int count = 1;
    for (int base = 1; base <= nbases; ++base) {
        int nzones = 0;
        if (CGNS_LOG_CALL(cg_nzones(this->m_file_id, base, &nzones)) != CG_OK) {
            continue;
        }
        if (nzones == 0) {
            continue;
        }

        auto& zone_indices = this->m_base_zone_indices.try_emplace(base, BaseZone { .index_base = base }).first->second;
        zone_indices.zone_indices.reserve(nzones);
        for (int index_zone = 1; index_zone <= nzones; ++index_zone) {
            zone_indices.zone_indices.emplace_back(index_zone);
        }

        char base_name[CGNS_NAME_MAX_LEN] = { };
        int base_cell_dim = 0, base_phys_dim = 0;
        CGNS_LOG_CALL(cg_base_read(this->get_file_id(), base, base_name, &base_cell_dim, &base_phys_dim));
        std::string base_name_str = base_name;
        if (base_name_str.empty()) {
            base_name_str = std::format("Step_{}", count++);
        }

        while (this->m_base_zone_layout.contains(base_name_str)) {
            base_name_str = std::format("{}_{}_{}", base_name_str, base, count++);
        }

        this->m_base_zone_layout.try_emplace(base_name_str, &zone_indices);
    }

    if (this->m_base_zone_indices.empty() || this->m_base_zone_layout.empty()) {
        LOG_ERROR("Base/Zone is empty.");
        return false;
    }
    return true;
}
