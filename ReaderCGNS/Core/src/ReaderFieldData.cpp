#include "ReaderFieldData.h"

#include "Utils/Utils.hpp"

bool ReaderFieldData::GetAllFieldFunctionName(std::vector<std::string>& field_names)
{
    if (this->m_field_layout.empty()) {
        if (!this->initialize_field_layout()) {
            return false;
        }
    }

    for (auto& name : this->m_field_layout | std::views::keys) {
        field_names.emplace_back(name);
    }

    if (field_names.empty()) {
        LOG_WARN("Field Function is empty.");
    }
    return true;
}

void ReaderFieldData::clear_field_data() noexcept
{
    utils::DeepClear(this->m_field_layout);
}

bool ReaderFieldData::initialize_field_layout()
{
    if (!this->IsOpen()) {
        LOG_ERROR("Cannot initialize grid topology without an open CGNS file.");
        return false;
    }

    decltype(this->m_field_layout) loaded_field_layout;
    const auto base_zone_indices = this->get_base_zone_indices();
    loaded_field_layout.reserve(base_zone_indices.size());

    for (const auto& [index_base, zone_indices] : base_zone_indices) {
        for (auto& index_zone : zone_indices) {
            int nsols = 0;
            CGNS_LOG_CALL(cg_nsols(this->get_file_id(), index_base, index_zone, &nsols));
            if (nsols < 1) {
                continue;
            }

            for (int index_sol = 1; index_sol <= nsols; ++index_sol) {
                char sol_name[CGNS_NAME_MAX_LEN] = { };
                int sol_nfields = 0;
                if (CGNS_LOG_CALL(cg_nfields(this->get_file_id(), index_base, index_zone, index_sol, &sol_nfields)) != CG_OK) {
                    continue;
                }

                for (int index_field = 1; index_field <= sol_nfields; ++index_field) {
                    char field_name[CGNS_NAME_MAX_LEN] = { };
                    CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;

                    if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index_base, index_zone, index_sol, index_field, &field_data_type, field_name)) != CG_OK) {
                        continue;
                    }

                    loaded_field_layout[field_name].emplace_back(FieldIndex { .base = index_base, .zone = index_zone, .solution = index_sol, .field = index_field });
                }
            }
        }
    }

    if (loaded_field_layout.empty()) {
        LOG_INFO("initialize_field_layout() failed.");
        return false;
    }

    this->m_field_layout = std::move(loaded_field_layout);
    return true;
}
