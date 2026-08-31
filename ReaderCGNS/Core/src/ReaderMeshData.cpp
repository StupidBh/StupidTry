#include "ReaderMeshData.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace {
    constexpr std::size_t ParentValuesPerElement = 4;

    bool IsVariableElementType(const CG_ElementType_t element_type) noexcept
    {
        return element_type == CG_ElementType_t::CG_MIXED || element_type == CG_ElementType_t::CG_NGON_n || element_type == CG_ElementType_t::CG_NFACE_n;
    }
} // namespace

bool ReaderMeshData::initialize_file_data()
{
    return this->initialize_grid_topology();
}

void ReaderMeshData::clear_file_data() noexcept
{
    decltype(this->m_grid_topology)().swap(this->m_grid_topology);
}

bool ReaderMeshData::initialize_grid_topology()
{
    if (!this->IsOpen()) {
        LOG_ERROR("Cannot initialize grid topology without an open CGNS file.");
        return false;
    }

    std::vector<BaseTopology> loaded_topology;
    const auto base_zone_indices = this->get_base_zone_indices();
    loaded_topology.reserve(base_zone_indices.size());

    for (const auto& [base_index, zone_indices] : base_zone_indices) {
        BaseTopology base;
        if (!this->read_base_topology(base_index, zone_indices, base)) {
            return false;
        }
        loaded_topology.emplace_back(std::move(base));
    }

    this->m_grid_topology = std::move(loaded_topology);
    return true;
}

bool ReaderMeshData::read_base_topology(const int base_index, const std::span<const int> zone_indices, BaseTopology& base) const
{
    base.index = base_index;
    char base_name[CGNS_NAME_MAX_LEN] = { };
    if (CGNS_LOG_CALL(cg_base_read(this->get_file_id(), base_index, base_name, &base.cell_dimension, &base.physical_dimension)) != CG_OK) {
        return false;
    }
    base.name = base_name;
    base.zones.reserve(zone_indices.size());

    for (const int zone_index : zone_indices) {
        ZoneTopology zone;
        if (!this->read_zone_topology(base_index, base.cell_dimension, zone_index, zone)) {
            return false;
        }
        base.zones.emplace_back(std::move(zone));
    }
    return true;
}

bool ReaderMeshData::read_zone_topology(const int base_index, const int base_cell_dimension, const int zone_index, ZoneTopology& zone) const
{
    zone.index = zone_index;
    if (CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), base_index, zone_index, &zone.zone_type)) != CG_OK ||
        CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), base_index, zone_index, &zone.index_dimension)) != CG_OK) {
        return false;
    }
    if (zone.index_dimension < 1 || zone.index_dimension > 3) {
        LOG_ERROR("Invalid index dimension {} at Base {}/Zone {}.", zone.index_dimension, base_index, zone_index);
        return false;
    }

    std::vector<cgsize_t> zone_size(static_cast<std::size_t>(zone.index_dimension) * 3);
    char zone_name[CGNS_NAME_MAX_LEN] = { };
    if (CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), base_index, zone_index, zone_name, zone_size.data())) != CG_OK) {
        return false;
    }
    zone.name = zone_name;

    if (zone.zone_type == CG_ZoneType_t::CG_Structured) {
        if (zone.index_dimension != base_cell_dimension) {
            LOG_ERROR("Structured Base {}/Zone {} has index dimension {}, expected {}.", base_index, zone_index, zone.index_dimension, base_cell_dimension);
            return false;
        }
        StructuredZoneTopology structured;
        if (!this->read_structured_zone_topology(zone.index_dimension, zone_size, structured)) {
            return false;
        }
        zone.data = std::move(structured);
        return true;
    }

    if (zone.zone_type == CG_ZoneType_t::CG_Unstructured) {
        if (zone.index_dimension != 1) {
            LOG_ERROR("Unstructured Base {}/Zone {} has index dimension {}, expected 1.", base_index, zone_index, zone.index_dimension);
            return false;
        }
        UnstructuredZoneTopology unstructured;
        if (!this->read_unstructured_zone_topology(base_index, zone_index, zone_size, unstructured)) {
            return false;
        }
        zone.data = std::move(unstructured);
        return true;
    }

    LOG_ERROR("Unsupported zone type [{}] at Base {}/Zone {}.", cg_ZoneTypeName(zone.zone_type), base_index, zone_index);
    return false;
}

bool ReaderMeshData::read_structured_zone_topology(const int index_dimension, const std::span<const cgsize_t> zone_size, StructuredZoneTopology& topology) const
{
    const std::size_t dimension = static_cast<std::size_t>(index_dimension);
    if (zone_size.size() != dimension * 3 || std::ranges::any_of(zone_size, [](const cgsize_t value) { return value < 0; })) {
        LOG_ERROR("Invalid structured zone dimensions.");
        return false;
    }

    topology.vertex_size.assign(zone_size.begin(), zone_size.begin() + dimension);
    topology.cell_size.assign(zone_size.begin() + dimension, zone_size.begin() + dimension * 2);
    topology.boundary_vertex_size.assign(zone_size.begin() + dimension * 2, zone_size.end());
    return true;
}

bool ReaderMeshData::read_unstructured_zone_topology(const int base_index,
                                                     const int zone_index,
                                                     const std::span<const cgsize_t> zone_size,
                                                     UnstructuredZoneTopology& topology) const
{
    if (zone_size.size() != 3 || std::ranges::any_of(zone_size, [](const cgsize_t value) { return value < 0; })) {
        LOG_ERROR("Invalid unstructured zone dimensions at Base {}/Zone {}.", base_index, zone_index);
        return false;
    }
    topology.vertex_count = zone_size[0];
    topology.cell_count = zone_size[1];
    topology.boundary_vertex_count = zone_size[2];

    int section_count = 0;
    if (CGNS_LOG_CALL(cg_nsections(this->get_file_id(), base_index, zone_index, &section_count)) != CG_OK || section_count < 0) {
        return false;
    }
    topology.sections.reserve(static_cast<std::size_t>(section_count));

    for (int section_index = 1; section_index <= section_count; ++section_index) {
        SectionTopology section;
        if (!this->read_section_topology(base_index, zone_index, section_index, section)) {
            return false;
        }
        topology.sections.emplace_back(std::move(section));
    }
    return true;
}

bool ReaderMeshData::read_section_topology(const int base_index, const int zone_index, const int section_index, SectionTopology& section) const
{
    section.index = section_index;
    char section_name[CGNS_NAME_MAX_LEN] = { };
    int parent_flag = 0;
    if (CGNS_LOG_CALL(cg_section_read(this->get_file_id(),
                                      base_index,
                                      zone_index,
                                      section_index,
                                      section_name,
                                      &section.element_type,
                                      &section.range_start,
                                      &section.range_end,
                                      &section.boundary_element_count,
                                      &parent_flag)) != CG_OK) {
        return false;
    }
    section.name = section_name;
    section.has_parent_data = parent_flag != 0;

    if (section.range_start < 1 || section.range_end < section.range_start) {
        LOG_ERROR("Invalid element range [{}, {}] at Base {}/Zone {}/Section {}.", section.range_start, section.range_end, base_index, zone_index, section_index);
        return false;
    }
    const cgsize_t element_count_value = section.range_end - section.range_start + 1;
    if (!std::in_range<std::size_t>(element_count_value)) {
        LOG_ERROR("Element count exceeds addressable memory at Base {}/Zone {}/Section {}.", base_index, zone_index, section_index);
        return false;
    }
    const std::size_t element_count = static_cast<std::size_t>(element_count_value);

    cgsize_t element_data_size = 0;
    if (CGNS_LOG_CALL(cg_ElementDataSize(this->get_file_id(), base_index, zone_index, section_index, &element_data_size)) != CG_OK || element_data_size < 0 ||
        !std::in_range<std::size_t>(element_data_size)) {
        return false;
    }

    if (section.has_parent_data) {
        if (element_count > std::numeric_limits<std::size_t>::max() / ParentValuesPerElement) {
            LOG_ERROR("Parent data exceeds addressable memory at Base {}/Zone {}/Section {}.", base_index, zone_index, section_index);
            return false;
        }
        section.parent_data.resize(element_count * ParentValuesPerElement);
    }

    if (IsVariableElementType(section.element_type)) {
        return this->read_variable_connectivity(base_index, zone_index, section_index, element_count, element_data_size, section);
    }
    return this->read_fixed_connectivity(base_index, zone_index, section_index, element_count, element_data_size, section);
}

bool ReaderMeshData::read_fixed_connectivity(const int base_index,
                                             const int zone_index,
                                             const int section_index,
                                             const std::size_t element_count,
                                             const cgsize_t element_data_size,
                                             SectionTopology& section) const
{
    FixedElementConnectivity connectivity;
    if (CGNS_LOG_CALL(cg_npe(section.element_type, &connectivity.nodes_per_element)) != CG_OK || connectivity.nodes_per_element <= 0) {
        LOG_ERROR("Unsupported fixed element type [{}] at Base {}/Zone {}/Section {}.",
                  cg_ElementTypeName(section.element_type),
                  base_index,
                  zone_index,
                  section_index);
        return false;
    }

    const std::size_t nodes_per_element = static_cast<std::size_t>(connectivity.nodes_per_element);
    if (element_count > std::numeric_limits<std::size_t>::max() / nodes_per_element ||
        element_count * nodes_per_element != static_cast<std::size_t>(element_data_size)) {
        LOG_ERROR("Connectivity size does not match element type [{}] at Base {}/Zone {}/Section {}.",
                  cg_ElementTypeName(section.element_type),
                  base_index,
                  zone_index,
                  section_index);
        return false;
    }

    connectivity.values.resize(static_cast<std::size_t>(element_data_size));
    cgsize_t* parent_data = section.parent_data.empty() ? nullptr : section.parent_data.data();
    if (CGNS_LOG_CALL(cg_elements_read(this->get_file_id(), base_index, zone_index, section_index, connectivity.values.data(), parent_data)) != CG_OK) {
        return false;
    }
    section.connectivity = std::move(connectivity);
    return true;
}

bool ReaderMeshData::read_variable_connectivity(const int base_index,
                                                const int zone_index,
                                                const int section_index,
                                                const std::size_t element_count,
                                                const cgsize_t element_data_size,
                                                SectionTopology& section) const
{
    if (element_count == std::numeric_limits<std::size_t>::max()) {
        LOG_ERROR("Connectivity offsets exceed addressable memory at Base {}/Zone {}/Section {}.", base_index, zone_index, section_index);
        return false;
    }

    VariableElementConnectivity connectivity;
    connectivity.values.resize(static_cast<std::size_t>(element_data_size));
    connectivity.offsets.resize(element_count + 1);
    cgsize_t* parent_data = section.parent_data.empty() ? nullptr : section.parent_data.data();
    if (CGNS_LOG_CALL(cg_poly_elements_read(this->get_file_id(),
                                            base_index,
                                            zone_index,
                                            section_index,
                                            connectivity.values.data(),
                                            connectivity.offsets.data(),
                                            parent_data)) != CG_OK) {
        return false;
    }
    if (connectivity.offsets.front() != 0 || connectivity.offsets.back() != element_data_size || !std::ranges::is_sorted(connectivity.offsets)) {
        LOG_ERROR("Invalid connectivity offsets at Base {}/Zone {}/Section {}.", base_index, zone_index, section_index);
        return false;
    }

    section.connectivity = std::move(connectivity);
    return true;
}
