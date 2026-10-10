#include "ReaderFieldData.h"
#include "CgnsTypes.hpp"

#include <format>
#include <limits>
#include <span>

#include "Utils/Utils.hpp"

namespace {
    bool CheckedProduct(const std::span<const cgsize_t> dimensions, cgsize_t& product) noexcept
    {
        product = 1;
        for (const cgsize_t dimension : dimensions) {
            if (dimension <= 0 || product > std::numeric_limits<cgsize_t>::max() / dimension) {
                return false;
            }
            product *= dimension;
        }
        return true;
    }
} // namespace

bool ReaderFieldData::GetAllFieldFunctionName(std::vector<ReaderAPI::Field>& field_names)
{
    if (!this->m_field_layout_initialized && !this->initialize_field_layout()) {
        return false;
    }

    std::vector<ReaderAPI::Field> loaded_fields;
    loaded_fields.reserve(this->m_field_groups.size());
    for (const auto& [name, group] : this->m_field_groups) {
        auto sub_vars = group.fields | std::views::keys | std::ranges::to<std::vector<std::string>>();
        std::ranges::sort(sub_vars);
        loaded_fields.emplace_back(name, std::move(sub_vars));
    }
    std::ranges::sort(loaded_fields, { }, &ReaderAPI::Field::var);
    if (loaded_fields.empty()) {
        LOG_WARN("Get field function data is empty.");
        return false;
    }

    field_names = std::move(loaded_fields);
    LOG_INFO("Get field function count={}", field_names.size());
    return true;
}

int ReaderFieldData::GetFieldFunctionPosition(const std::string& var, const std::string& sub_var)
{
    if (this->find_field_indices(var, sub_var) == nullptr) {
        return -1;
    }
    return this->m_field_groups.at(var).position;
}

bool ReaderFieldData::GetFieldFunctionIds(const std::string& var, const std::string& sub_var, std::vector<ReaderAPI::Integer>& ids)
{
    const auto* indices = this->find_field_indices(var, sub_var);
    if (indices == nullptr) {
        return false;
    }

    const int position = this->m_field_groups.at(var).position;
    for (const auto& index : *indices) {
        cgsize_t data_size = 1;
        const auto& offset = this->m_offset.at(index.base).at(index.zone);
        for (std::size_t i = 0; i < offset.r_max.size(); ++i) {
            data_size *= (offset.r_max[i] - ZoneOffset::r_min[i] + 1);
        }

        utils::AppendVector(ids, utils::CreateVector<ReaderAPI::Integer>(data_size, position == 0 ? offset.node_offset : offset.cell_offset, 1));
    }

    if (ids.empty()) {
        LOG_WARN("Field function [{}]-[{}] ids is empty.", var, sub_var);
        return false;
    }
    return true;
}

bool ReaderFieldData::GetFieldFunctionData(const std::string& var, const std::string& sub_var, std::vector<ReaderAPI::Real>& data)
{
    const auto* indices = this->find_field_indices(var, sub_var);
    if (indices == nullptr) {
        return false;
    }

    for (const auto& index : *indices) {
        char field_name[CGNS_NAME_MAX_LEN] = { };
        CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;
        if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index.base, index.zone, index.solution, index.field, &field_data_type, field_name)) != CG_OK) {
            continue;
        }

        cgsize_t data_size = 1;
        const auto& offset = this->m_offset.at(index.base).at(index.zone);
        for (std::size_t i = 0; i < offset.r_max.size(); ++i) {
            data_size *= (offset.r_max[i] - ZoneOffset::r_min[i] + 1);
        }
        std::vector<ReaderAPI::Real> loaded_values(data_size, 0);

        if (CGNS_LOG_CALL(cg_field_read(this->get_file_id(),
                                        index.base,
                                        index.zone,
                                        index.solution,
                                        field_name,
                                        CG_DataType_t::CG_RealSingle,
                                        offset.r_min.data(),
                                        offset.r_max.data(),
                                        loaded_values.data())) != CG_OK) {
            continue;
        }
        utils::AppendVector(data, std::move(loaded_values));
    }

    if (data.empty()) {
        LOG_WARN("Field function [{}]-[{}] values is empty.", var, sub_var);
        return false;
    }
    return true;
}

void ReaderFieldData::clear_field_data() noexcept
{
    utils::DeepClear(this->m_field_groups);
    utils::DeepClear(this->m_offset);
    this->m_field_layout_initialized = false;

    LOG_TRACE("[clear_field_data] finish.");
}

bool ReaderFieldData::initialize_field_layout()
{
    if (!this->IsOpen()) {
        LOG_ERROR("Cannot initialize field layout without an open CGNS file.");
        return false;
    }

    using PositionGroups = std::array<FieldIndices, 2>; // 0-Vertex | 1-CellCenter
    std::unordered_map<std::string, PositionGroups> loaded_field_layout;
    BaseZoneOffset loaded_offsets;

    cgsize_t node_offset = 0;
    cgsize_t cell_offset = 0;
    for (const auto& [index_base, zone_indices] : this->get_base_zone_indices()) {
        for (const int index_zone : zone_indices) {
            int index_zone_dim = 0;
            std::array<cgsize_t, 9> zone_size { };
            char zone_name[CGNS_NAME_MAX_LEN] = { };
            CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
            if (CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), index_base, index_zone, &zone_type)) != CG_OK ||
                CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), index_base, index_zone, &index_zone_dim)) != CG_OK ||
                CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), index_base, index_zone, zone_name, zone_size.data())) != CG_OK) {
                continue;
            }
            if ((zone_type != CG_ZoneType_t::CG_Structured && zone_type != CG_ZoneType_t::CG_Unstructured) || index_zone_dim < 1 || index_zone_dim > 3) {
                LOG_ERROR("Unsupported zone topology at Base {}/Zone {}.", index_base, index_zone);
                continue;
            }

            cgsize_t node_count = 0;
            cgsize_t cell_count = 0;
            if (!CheckedProduct(std::span(zone_size.data(), static_cast<std::size_t>(index_zone_dim)), node_count) ||
                !CheckedProduct(std::span(zone_size.data() + index_zone_dim, static_cast<std::size_t>(index_zone_dim)), cell_count)) {
                LOG_ERROR("Invalid zone size at Base {}/Zone {}.", index_base, index_zone);
                continue;
            }

            ZoneOffset index_offset;
            index_offset.node_offset = node_offset;
            index_offset.cell_offset = cell_offset;

            node_offset += node_count;
            cell_offset += cell_count;

            int nsolutions = 0;
            if (CGNS_LOG_CALL(cg_nsols(this->get_file_id(), index_base, index_zone, &nsolutions)) != CG_OK) {
                continue;
            }

            if (nsolutions < 1) {
                continue;
            }
            static constexpr int index_sol = 1;
            char sol_name[CGNS_NAME_MAX_LEN] = { };
            CG_GridLocation_t sol_location = CG_GridLocation_t::CG_GridLocationNull;
            if (CGNS_LOG_CALL(cg_sol_info(this->get_file_id(), index_base, index_zone, index_sol, sol_name, &sol_location)) != CG_OK) {
                continue;
            }
            if (sol_location != CG_GridLocation_t::CG_Vertex && sol_location != CG_GridLocation_t::CG_CellCenter) {
                LOG_WARN("Skip unsupported type [{}] by [{}] at Base {}/Zone {}/Sol {}",
                         cg_GridLocationName(sol_location),
                         sol_name,
                         index_base,
                         index_zone,
                         index_sol);
                continue;
            }

            index_offset.r_max.resize(index_zone_dim, 0);
            int solution_dim = 0;
            if (CGNS_LOG_CALL(cg_sol_size(this->get_file_id(), index_base, index_zone, index_sol, &solution_dim, index_offset.r_max.data())) != CG_OK) {
                return false;
            }
            loaded_offsets[index_base][index_zone] = std::move(index_offset);

            int nfields = 0;
            if (CGNS_LOG_CALL(cg_nfields(this->get_file_id(), index_base, index_zone, index_sol, &nfields)) != CG_OK) {
                continue;
            }
            const auto position = sol_location == CG_GridLocation_t::CG_Vertex ? 0 : 1;
            for (int index_field = 1; index_field <= nfields; ++index_field) {
                char field_name[CGNS_NAME_MAX_LEN] = { };
                CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;
                if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index_base, index_zone, index_sol, index_field, &field_data_type, field_name)) != CG_OK) {
                    continue;
                }
                if (field_data_type != CG_DataType_t::CG_RealDouble && field_data_type != CG_DataType_t::CG_RealSingle) {
                    LOG_WARN("Skip unsupported type [{}] by [{}] at Base {}/Zone {}/Sol {}/Field {}",
                             cg_DataTypeName(field_data_type),
                             field_name,
                             index_base,
                             index_zone,
                             index_sol,
                             index_field);
                    continue;
                }
                loaded_field_layout[field_name][position].emplace_back(
                    FieldIndex { .base = index_base, .zone = index_zone, .solution = index_sol, .field = index_field });
            }
        }
    }

    auto names = loaded_field_layout | std::views::keys | std::ranges::to<std::vector<std::string>>();
    std::ranges::sort(names);
    std::unordered_map<std::string, std::array<FieldGroup, 2>> grouped_fields;
    std::array<std::string, 2> current_groups;
    for (const auto& name : names) {
        auto& positions = loaded_field_layout.at(name);
        for (std::size_t position = 0; position < positions.size(); ++position) {
            if (positions[position].empty()) {
                continue;
            }
            auto& group_name = current_groups[position];
            const auto index = std::min(name.rfind("Magnitude"), name.rfind('_'));
            const bool starts_group = group_name.empty() || !name.starts_with(group_name);
            if (starts_group) {
                group_name = index == 0 ? name : name.substr(0, index);
            }
            auto& group = grouped_fields[group_name][position];
            group.position = static_cast<int>(position);
            group.fields.emplace(name, std::move(positions[position]));
            if (starts_group && index == std::string::npos) {
                group_name.clear();
            }
        }
    }

    std::unordered_map<std::string, FieldGroup> loaded_groups;
    for (auto& [name, positions] : grouped_fields) {
        const bool mixed_positions = !positions[0].fields.empty() && !positions[1].fields.empty();
        for (std::size_t position = 0; position < positions.size(); ++position) {
            if (positions[position].fields.empty()) {
                continue;
            }
            const auto suffix = mixed_positions ? (position == 0 ? "_vertex" : "_cellcenter") : "";
            if (!loaded_groups.emplace(name + suffix, std::move(positions[position])).second) {
                LOG_ERROR("Field group name [{}] is ambiguous.", name + suffix);
                return false;
            }
        }
    }

    this->m_offset = std::move(loaded_offsets);
    this->m_field_groups = std::move(loaded_groups);
    this->m_field_layout_initialized = true;
    return true;
}

const ReaderFieldData::FieldIndices* ReaderFieldData::find_field_indices(const std::string& var, const std::string& sub_var)
{
    if (!this->m_field_layout_initialized && !this->initialize_field_layout()) {
        return nullptr;
    }

    const auto group = this->m_field_groups.find(var);
    if (group != this->m_field_groups.end()) {
        const auto field = group->second.fields.find(sub_var);
        if (field != group->second.fields.end()) {
            return &field->second;
        }
    }
    LOG_ERROR("Field function [{}]-[{}] doesn't exist.", var, sub_var);
    return nullptr;
}
