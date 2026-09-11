#include "ReaderFieldData.h"

#include <format>
#include <limits>
#include <span>

#include "Utils/Utils.hpp"

namespace {
    constexpr ReaderAPI::Integer ToFieldType(const CG_GridLocation_t location) noexcept
    {
        switch (location) {
            case CG_GridLocation_t::CG_Vertex     : return 0;
            case CG_GridLocation_t::CG_CellCenter : return 1;
            case CG_GridLocation_t::CG_FaceCenter :
            case CG_GridLocation_t::CG_IFaceCenter:
            case CG_GridLocation_t::CG_JFaceCenter:
            case CG_GridLocation_t::CG_KFaceCenter: return 2;
            default                               : return -1;
        }
    }

    const char* FieldLocationSuffix(const CG_GridLocation_t location) noexcept
    {
        switch (location) {
            case CG_GridLocation_t::CG_Vertex    : return "Vertex";
            case CG_GridLocation_t::CG_CellCenter: return "CellCenter";
            default                              : return nullptr;
        }
    }

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

    bool CheckedAdd(const cgsize_t left, const cgsize_t right, cgsize_t& result) noexcept
    {
        if (right < 0 || left > std::numeric_limits<cgsize_t>::max() - right) {
            return false;
        }
        result = left + right;
        return true;
    }

    bool FitsFieldIdRange(const cgsize_t offset, const cgsize_t count) noexcept
    {
        constexpr auto MaxFieldId = static_cast<cgsize_t>(std::numeric_limits<ReaderAPI::Integer>::max());
        return count > 0 && offset <= MaxFieldId && count - 1 <= MaxFieldId - offset;
    }
} // namespace

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

bool ReaderFieldData::GetFieldFunctionData(const std::string& field_name, ReaderAPI::Field& field_data)
{
    if (this->m_field_layout.empty()) {
        if (!this->initialize_field_layout()) {
            return false;
        }
    }
    const auto field_iter = this->m_field_layout.find(field_name);
    if (field_iter == this->m_field_layout.end() || field_iter->second.empty()) {
        LOG_WARN("Field Function {} doesn't exist.", field_name);
        return false;
    }

    const auto& field_indices = field_iter->second;
    const CG_GridLocation_t field_location = field_indices.front().location;
    const ReaderAPI::Integer field_type = ToFieldType(field_location);
    if (field_type < 0 || field_type > 1) {
        LOG_ERROR("Field Function {} uses unsupported GridLocation [{}].", field_name, cg_GridLocationName(field_location));
        return false;
    }

    std::size_t total_value_count = 0;
    for (const auto& field_index : field_indices) {
        if (field_index.location != field_location) {
            LOG_ERROR("Field Function {} has inconsistent GridLocation across zones.", field_name);
            return false;
        }
        if (!std::in_range<std::size_t>(field_index.value_count) ||
            static_cast<std::size_t>(field_index.value_count) > std::numeric_limits<std::size_t>::max() - total_value_count) {
            LOG_ERROR("Field Function {} exceeds addressable memory.", field_name);
            return false;
        }
        total_value_count += static_cast<std::size_t>(field_index.value_count);
    }

    ReaderAPI::Field loaded_field_data { .name = field_name, .type = field_type };
    loaded_field_data.ids.reserve(total_value_count);
    loaded_field_data.values.reserve(total_value_count);

    for (const auto& field_index : field_indices) {
        std::array<cgsize_t, 3> range_min { 1, 1, 1 };
        std::vector<ReaderAPI::Real> loaded_values(static_cast<std::size_t>(field_index.value_count));

        if (CGNS_LOG_CALL(cg_field_read(this->get_file_id(),
                                        field_index.base,
                                        field_index.zone,
                                        field_index.solution,
                                        field_index.source_name.c_str(),
                                        CG_DataType_t::CG_RealSingle,
                                        range_min.data(),
                                        field_index.dimensions.data(),
                                        loaded_values.data())) != CG_OK) {
            return false;
        }

        for (cgsize_t index = 0; index < field_index.value_count; ++index) {
            loaded_field_data.ids.emplace_back(field_index.id_offset + static_cast<ReaderAPI::Integer>(index));
        }
        utils::AppendVector(loaded_field_data.values, std::move(loaded_values));
    }

    if (loaded_field_data.isEmpty()) {
        LOG_ERROR("Field Function {} produced invalid data.", field_name);
        return false;
    }
    field_data = std::move(loaded_field_data);
    return true;
}

bool ReaderFieldData::GetFieldFunctionData(const std::vector<std::string>& field_names, std::vector<ReaderAPI::Field>& field_data)
{
    std::vector<ReaderAPI::Field> loaded_fields;
    for (auto& name : field_names) {
        ReaderAPI::Field loaded_field;
        if (!this->GetFieldFunctionData(name, loaded_field)) {
            continue;
        }
        loaded_fields.emplace_back(std::move(loaded_field));
    }

    if (loaded_fields.empty()) {
        LOG_WARN("Field Function data is empty.");
        return false;
    }
    field_data = std::move(loaded_fields);
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

    using LocationFieldLayout = std::unordered_map<CG_GridLocation_t, FieldIndices>;
    std::unordered_map<std::string, LocationFieldLayout> grouped_field_layout;
    const auto base_zone_indices = this->get_base_zone_indices();
    grouped_field_layout.reserve(base_zone_indices.size());

    cgsize_t node_offset = 0;
    cgsize_t cell_offset = 0;

    for (const auto& [index_base, zone_indices] : base_zone_indices) {
        for (const int index_zone : zone_indices) {
            CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
            int index_dim = 0;
            std::array<cgsize_t, 9> zone_size { };
            char zone_name[CGNS_NAME_MAX_LEN] = { };
            if (CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), index_base, index_zone, &zone_type)) != CG_OK ||
                CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), index_base, index_zone, &index_dim)) != CG_OK ||
                CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), index_base, index_zone, zone_name, zone_size.data())) != CG_OK) {
                return false;
            }
            if ((zone_type != CG_ZoneType_t::CG_Structured && zone_type != CG_ZoneType_t::CG_Unstructured) || index_dim < 1 || index_dim > 3) {
                LOG_ERROR("Unsupported zone topology at Base {}/Zone {}.", index_base, index_zone);
                return false;
            }

            cgsize_t node_count = 0;
            cgsize_t cell_count = 0;
            if (!CheckedProduct(std::span(zone_size.data(), static_cast<std::size_t>(index_dim)), node_count) ||
                !CheckedProduct(std::span(zone_size.data() + index_dim, static_cast<std::size_t>(index_dim)), cell_count)) {
                LOG_ERROR("Invalid zone size at Base {}/Zone {}.", index_base, index_zone);
                return false;
            }

            const cgsize_t zone_node_offset = node_offset;
            const cgsize_t zone_cell_offset = cell_offset;
            if (!CheckedAdd(node_offset, node_count, node_offset) || !CheckedAdd(cell_offset, cell_count, cell_offset)) {
                LOG_ERROR("Global entity count overflow at Base {}/Zone {}.", index_base, index_zone);
                return false;
            }

            int nsols = 0;
            if (CGNS_LOG_CALL(cg_nsols(this->get_file_id(), index_base, index_zone, &nsols)) != CG_OK) {
                return false;
            }
            if (nsols < 1) {
                continue;
            }
            if (nsols > 1) {
                LOG_ERROR("Expected at most one FlowSolution at Base {}/Zone {}, got {}.", index_base, index_zone, nsols);
                return false;
            }

            constexpr int IndexSolution = 1;
            char solution_name[CGNS_NAME_MAX_LEN] = { };
            CG_GridLocation_t solution_location = CG_GridLocation_t::CG_GridLocationNull;
            if (CGNS_LOG_CALL(cg_sol_info(this->get_file_id(), index_base, index_zone, IndexSolution, solution_name, &solution_location)) != CG_OK) {
                return false;
            }
            const ReaderAPI::Integer field_type = ToFieldType(solution_location);
            if (field_type < 0 || field_type > 1) {
                LOG_ERROR("Unsupported FlowSolution GridLocation [{}] at Base {}/Zone {}.", cg_GridLocationName(solution_location), index_base, index_zone);
                return false;
            }

            int data_dim = 0;
            std::array<cgsize_t, 3> dimensions { };
            if (CGNS_LOG_CALL(cg_sol_size(this->get_file_id(), index_base, index_zone, IndexSolution, &data_dim, dimensions.data())) != CG_OK) {
                return false;
            }
            if (data_dim < 1 || data_dim > index_dim || data_dim > static_cast<int>(dimensions.size())) {
                LOG_ERROR("Invalid FlowSolution dimension {} at Base {}/Zone {}.", data_dim, index_base, index_zone);
                return false;
            }

            cgsize_t value_count = 0;
            if (!CheckedProduct(std::span(dimensions.data(), static_cast<std::size_t>(data_dim)), value_count)) {
                LOG_ERROR("Invalid FlowSolution size at Base {}/Zone {}.", index_base, index_zone);
                return false;
            }
            const cgsize_t expected_value_count = solution_location == CG_GridLocation_t::CG_Vertex ? node_count : cell_count;
            const cgsize_t id_offset = solution_location == CG_GridLocation_t::CG_Vertex ? zone_node_offset : zone_cell_offset;
            if (value_count != expected_value_count) {
                LOG_ERROR("FlowSolution size {} does not match {} count {} at Base {}/Zone {}.",
                          value_count,
                          cg_GridLocationName(solution_location),
                          expected_value_count,
                          index_base,
                          index_zone);
                return false;
            }
            if (!FitsFieldIdRange(id_offset, value_count)) {
                LOG_ERROR("Field entity IDs exceed the ReaderAPI::Integer range at Base {}/Zone {}.", index_base, index_zone);
                return false;
            }

            int solution_field_count = 0;
            if (CGNS_LOG_CALL(cg_nfields(this->get_file_id(), index_base, index_zone, IndexSolution, &solution_field_count)) != CG_OK) {
                return false;
            }

            for (int index_field = 1; index_field <= solution_field_count; ++index_field) {
                char field_name[CGNS_NAME_MAX_LEN] = { };
                CG_DataType_t field_data_type = CG_DataType_t::CG_DataTypeNull;
                if (CGNS_LOG_CALL(cg_field_info(this->get_file_id(), index_base, index_zone, IndexSolution, index_field, &field_data_type, field_name)) != CG_OK) {
                    return false;
                }

                grouped_field_layout[field_name][solution_location].emplace_back(FieldIndex { .base = index_base,
                                                                                              .zone = index_zone,
                                                                                              .solution = IndexSolution,
                                                                                              .field = index_field,
                                                                                              .source_name = field_name,
                                                                                              .location = solution_location,
                                                                                              .id_offset = static_cast<ReaderAPI::Integer>(id_offset),
                                                                                              .value_count = value_count,
                                                                                              .data_dim = data_dim,
                                                                                              .dimensions = dimensions });
            }
        }
    }

    decltype(this->m_field_layout) loaded_field_layout;
    loaded_field_layout.reserve(grouped_field_layout.size());
    for (auto& [source_name, location_layout] : grouped_field_layout) {
        if (location_layout.size() == 1) {
            if (!loaded_field_layout.try_emplace(source_name, std::move(location_layout.begin()->second)).second) {
                LOG_ERROR("Field Function name conflicts with an existing qualified field: {}.", source_name);
                return false;
            }
            continue;
        }

        for (auto& [location, field_indices] : location_layout) {
            const char* location_suffix = FieldLocationSuffix(location);
            if (location_suffix == nullptr) {
                LOG_ERROR("Cannot qualify Field Function {} with unsupported GridLocation [{}].", source_name, cg_GridLocationName(location));
                return false;
            }

            std::string qualified_name = std::format("{}_{}", source_name, location_suffix);
            if (!loaded_field_layout.try_emplace(std::move(qualified_name), std::move(field_indices)).second) {
                LOG_ERROR("Qualified Field Function name conflicts with an existing field: {}_{}.", source_name, location_suffix);
                return false;
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
