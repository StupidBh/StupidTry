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
    if (this->m_field_layout.empty()) {
        if (!this->initialize_field_layout()) {
            return false;
        }
    }

    std::vector<ReaderAPI::Field> loaded_fields;
    ReaderAPI::Field loaded_field;

    auto keys = this->m_field_layout | std::views::keys | std::ranges::to<std::vector<std::string>>();
    std::ranges::sort(keys);

    for (auto& field_name : keys) {
        if (!loaded_field.var.empty()) {
            if (field_name.starts_with(loaded_field.var)) {
                loaded_field.sub_vars.emplace_back(field_name);
                continue;
            }
            loaded_fields.emplace_back(loaded_field);
            loaded_field.var.clear();
            loaded_field.sub_vars.clear();
        }

        const auto index = std::min(field_name.rfind("Magnitude"), field_name.rfind('_'));
        if (index != std::string::npos) {
            loaded_field.var = field_name.substr(0, index);
            loaded_field.sub_vars.emplace_back(field_name);
        }
        else {
            loaded_field.var = field_name;
            loaded_field.sub_vars.emplace_back(field_name);

            loaded_fields.emplace_back(loaded_field);
            loaded_field.var.clear();
            loaded_field.sub_vars.clear();
        }
    }

    if (!loaded_field.var.empty()) {
        loaded_fields.emplace_back(loaded_field);
    }

    field_names = std::move(loaded_fields);
    return true;
}

bool ReaderFieldData::GetFieldFunctionData(const std::string& var, const std::string& sub_var, std::vector<ReaderAPI::Real>& data)
{
    if (this->m_field_layout.empty()) {
        if (!this->initialize_field_layout()) {
            return false;
        }
    }

    auto iter = this->m_field_layout.find(sub_var);
    if (iter == this->m_field_layout.end()) {
        LOG_ERROR("Field function [{}]-[{}] doesn't exists.", var, sub_var);
        return false;
    }

    for (const auto& index : iter->second) {
        char field_name[CGNS_NAME_MAX_LEN] = { };
        CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;
        if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index.base, index.zone, index.solution, index.field, &field_data_type, field_name)) != CG_OK) {
            continue;
        }

        cgsize_t data_size = 1;
        const auto& offset = this->m_offset[index.base][index.zone];
        for (std::size_t i = 0; i < offset.r_max.size(); ++i) {
            data_size *= (offset.r_max[i] - offset.r_min[i] + 1);
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
        LOG_WARN("Field function [{}]-[{}] is empty.", var, sub_var);
    }
    return true;
}

void ReaderFieldData::clear_field_data() noexcept
{
    utils::DeepClear(this->m_field_layout);

    LOG_TRACE("[clear_field_data] finish.");
}

bool ReaderFieldData::initialize_field_layout()
{
    if (!this->IsOpen()) {
        LOG_ERROR("Cannot initialize grid topology without an open CGNS file.");
        return false;
    }
    this->m_field_layout.clear();

    using PositionGroups = std::array<FieldIndices, 2>; // 0-Vertex | 1-CellCenter
    std::unordered_map<std::string, PositionGroups> loaded_field_layout;

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

            int temp_check_bit = 0;
            if (CGNS_LOG_CALL(cg_nsols(this->get_file_id(), index_base, index_zone, &temp_check_bit)) != CG_OK) {
                continue;
            }
            if (temp_check_bit < 1) {
                continue;
            }
            static constexpr int index_sol = 1;

            ZoneOffset index_offset;
            index_offset.r_max.resize(index_zone_dim, 0);
            if (CGNS_LOG_CALL(cg_sol_size(this->get_file_id(), index_base, index_zone, index_sol, &temp_check_bit, index_offset.r_max.data())) != CG_OK) {
                return false;
            }

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

            int nfields = 0;
            if (CGNS_LOG_CALL(cg_nfields(this->get_file_id(), index_base, index_zone, index_sol, &nfields)) != CG_OK) {
                continue;
            }

            bool flag = false;
            for (int index_field = 1; index_field <= nfields; ++index_field) {
                char field_name[CGNS_NAME_MAX_LEN] = { };
                CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;
                if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index_base, index_zone, index_sol, index_field, &field_data_type, field_name)) != CG_OK) {
                    continue;
                }
                if (field_data_type != CG_DataType_t::CG_RealDouble && field_data_type != CG_DataType_t::CG_RealSingle) {
                    LOG_WARN("Skip unsupported type [{}] by [{}] at Base {}/Zone {}/Sol {}/Field {} ",
                             cg_DataTypeName(field_data_type),
                             field_name,
                             index_base,
                             index_zone,
                             index_sol,
                             index_field);
                    continue;
                }

                const auto position = sol_location == CG_GridLocation_t::CG_Vertex ? 0 : 1;
                loaded_field_layout[field_name][position].emplace_back(
                    FieldIndex { .base = index_base, .zone = index_zone, .solution = index_sol, .field = index_field });

                flag = true;
            }

            if (flag) { // 当前 Base/Zone/sol 有真实存在的 Field 字段
                cgsize_t node_count = 0;
                cgsize_t cell_count = 0;
                if (!CheckedProduct(std::span(zone_size.data(), static_cast<std::size_t>(index_zone_dim)), node_count) ||
                    !CheckedProduct(std::span(zone_size.data() + index_zone_dim, static_cast<std::size_t>(index_zone_dim)), cell_count)) {
                    LOG_ERROR("Invalid zone size at Base {}/Zone {}.", index_base, index_zone);
                    continue;
                }

                index_offset.node_offset = node_offset;
                index_offset.cell_offset = cell_offset;
                this->m_offset[index_base][index_zone] = std::move(index_offset);
                this->m_solution_location[index_base][index_zone][index_sol] = sol_location;

                node_offset += node_count;
                cell_offset += cell_count;
            }
        }
    }

    for (auto& [name, groups] : loaded_field_layout) {
        auto& [vertex, cell_center] = groups;

        if (!vertex.empty() && !cell_center.empty()) {
            this->m_field_layout.emplace(name + "_Vertex", std::move(vertex));
            this->m_field_layout.emplace(name + "_CellCenter", std::move(cell_center));
        }
        else if (!vertex.empty()) {
            this->m_field_layout.emplace(name, std::move(vertex));
        }
        else {
            this->m_field_layout.emplace(name, std::move(cell_center));
        }
    }

    return true;
}
