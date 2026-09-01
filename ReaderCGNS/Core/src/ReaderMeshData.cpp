#include "ReaderMeshData.h"

#include <limits>
#include <source_location>

namespace {
    constexpr bool IsVariableElementType(const CG_ElementType_t element_type) noexcept
    {
        return element_type == CG_ElementType_t::CG_MIXED || element_type == CG_ElementType_t::CG_NGON_n || element_type == CG_ElementType_t::CG_NFACE_n;
    }
} // namespace

bool ReaderMeshData::GetAllElementSetName(std::vector<std::string>& element_set_names)
{
    if (this->m_grid_topology.empty()) {
        if (!this->initialize_grid_topology()) {
            LOG_INFO("initialize_grid_topology() failed.");
            return false;
        }
    }

    for (auto& grid_topology : this->m_grid_topology) {
        std::string_view base_name = grid_topology.name;
        for (auto& zone_topology : grid_topology.zones) {
            std::string_view zone_name = zone_topology.name;

            if (zone_topology.type == CG_ZoneType_t::CG_Structured || zone_topology.sections.empty()) {
                element_set_names.emplace_back(std::format("{}.{}", base_name, zone_name));
            }
            else {
                for (auto& section_topology : zone_topology.sections) {
                    element_set_names.emplace_back(std::format("{}.{}.{}", base_name, zone_name, section_topology.name));
                }
            }
        }
    }

    return !element_set_names.empty();
}

void ReaderMeshData::clear_grid_topology() noexcept
{
    utils::DeepClear(this->m_grid_topology);
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
            continue;
        }
        loaded_topology.emplace_back(std::move(base));
    }

    this->m_grid_topology = std::move(loaded_topology);
    return !this->m_grid_topology.empty();
}

bool ReaderMeshData::read_base_topology(const int index_base, const std::span<const int> zone_indices, BaseTopology& base) const
{
    char base_name[CGNS_NAME_MAX_LEN] = { };
    if (CGNS_LOG_CALL(cg_base_read(this->get_file_id(), index_base, base_name, &base.cell_dim, &base.phy_dim)) != CG_OK) {
        return false;
    }

    base.index = index_base;
    base.name = base_name;

    CG_SimulationType_t simulation_type = CG_SimulationType_t::CG_SimulationTypeNull;
    const int simulation_type_status = cg_simulation_type_read(this->get_file_id(), index_base, &simulation_type);
    if (simulation_type_status == CG_OK) {
        base.type = simulation_type;
    }
    else if (simulation_type_status != CG_NODE_NOT_FOUND) {
        this->GetLogDispatcher().HandleCgnsStatus(simulation_type_status, "cg_simulation_type_read", std::source_location::current());
    }

    base.zones.reserve(zone_indices.size());
    for (const int zone_index : zone_indices) {
        ZoneTopology zone;
        if (!this->read_zone_topology(index_base, zone_index, zone)) {
            continue;
        }
        base.zones.emplace_back(std::move(zone));
    }
    return true;
}

bool ReaderMeshData::read_zone_topology(const int index_base, const int index_zone, ZoneTopology& zone) const
{
    if (CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), index_base, index_zone, &zone.type)) != CG_OK ||
        CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), index_base, index_zone, &zone.dim)) != CG_OK) {
        return false;
    }

    std::size_t active_zone_size = 0;
    if (zone.type == CG_ZoneType_t::CG_Structured) {
        if (zone.dim < 1 || zone.dim > 3) {
            LOG_ERROR("Invalid structured index dimension {} at Base {}/Zone {}.", zone.dim, index_base, index_zone);
            return false;
        }
        active_zone_size = static_cast<std::size_t>(zone.dim) * 3;
    }
    else if (zone.type == CG_ZoneType_t::CG_Unstructured) {
        active_zone_size = 3;
    }
    else {
        LOG_ERROR("Unsupported zone type [{}] at Base {}/Zone {}.", cg_ZoneTypeName(zone.type), index_base, index_zone);
        return false;
    }

    char zone_name[CGNS_NAME_MAX_LEN] = { };
    if (CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), index_base, index_zone, zone_name, zone.zone_size.data())) != CG_OK) {
        return false;
    }

    const std::span<const cgsize_t> active_size(zone.zone_size.data(), active_zone_size);
    if (std::ranges::any_of(active_size, [](const cgsize_t value) { return value < 0; })) {
        LOG_ERROR("Invalid zone dimensions at Base {}/Zone {}.", index_base, index_zone);
        return false;
    }

    zone.index = index_zone;
    zone.name = zone_name;

    if (!read_zone_coordinates(index_base, index_zone, zone)) {
        return false;
    }

    if (zone.type == CG_ZoneType_t::CG_Structured) {
        return this->build_structured_section(zone);
    }

    this->read_unstructured_zone_sections(index_base, index_zone, zone);
    return true;
}

bool ReaderMeshData::read_zone_coordinates(int index_base, int index_zone, ZoneTopology& zone) const
{
    int zone_ncoords = 0;
    if (CGNS_LOG_CALL(cg_ncoords(this->get_file_id(), index_base, index_zone, &zone_ncoords)) != CG_OK) {
        return false;
    }
    if (zone_ncoords < 1 || zone_ncoords > 3) {
        LOG_ERROR("Invalid zone ncoords {} at Base {}/Zone {}.", zone_ncoords, index_base, index_zone);
        return false;
    }

    const std::vector<cgsize_t> r_min(zone.dim, 1);
    std::vector<cgsize_t> r_max(zone.dim, 1);
    cgsize_t vertex_sum = 1;
    for (int i = 0; i < zone.dim; ++i) {
        r_max[i] = zone.zone_size[i];
        vertex_sum *= zone.zone_size[i];
    }
    zone.coordinates_xyz.fill(std::vector(vertex_sum, 0.F));

    for (int index_coord = 1; index_coord <= zone_ncoords; ++index_coord) {
        char index_coord_name[CGNS_NAME_MAX_LEN] = { };
        CG_DataType_t index_coord_type = CG_DataType_t::CG_DataTypeNull;
        if (CGNS_LOG_CALL(cg_coord_info(this->get_file_id(), index_base, index_zone, index_coord, &index_coord_type, index_coord_name)) != CG_OK) {
            continue;
        }

        if (index_coord_type == CG_DataType_t::CG_RealSingle) {
            CGNS_LOG_CALL(cg_coord_read(this->get_file_id(),
                                        index_base,
                                        index_zone,
                                        index_coord_name,
                                        index_coord_type,
                                        r_min.data(),
                                        r_max.data(),
                                        zone.coordinates_xyz[index_coord - 1].data()));
        }
        else if (index_coord_type == CG_DataType_t::CG_RealDouble) {
            std::vector<double> temp_buff(vertex_sum, 0.0);

            CGNS_LOG_CALL(
                cg_coord_read(this->get_file_id(), index_base, index_zone, index_coord_name, index_coord_type, r_min.data(), r_max.data(), temp_buff.data()));
            zone.coordinates_xyz[index_coord - 1] = utils::ShrinkVector<float>(temp_buff);
        }
        else {
            LOG_WARN("[ZoneCoords]{:>2}:[{}] {}, Unknown data-type.", index_coord, cg_DataTypeName(index_coord_type), index_coord_name);
        }
    }

    return true;
}

void ReaderMeshData::read_unstructured_zone_sections(const int index_base, const int index_zone, ZoneTopology& zone) const
{
    int section_count = 0;
    if (CGNS_LOG_CALL(cg_nsections(this->get_file_id(), index_base, index_zone, &section_count)) != CG_OK) {
        return;
    }
    if (section_count < 0) {
        LOG_ERROR("Invalid section count {} at Base {}/Zone {}.", section_count, index_base, index_zone);
        return;
    }

    zone.sections.reserve(static_cast<std::size_t>(section_count));
    for (int index_section = 1; index_section <= section_count; ++index_section) {
        SectionTopology section;
        if (!this->read_section_topology(index_base, index_zone, index_section, section)) {
            continue;
        }
        zone.sections.emplace_back(std::move(section));
    }
}

bool ReaderMeshData::read_section_topology(const int index_base, const int index_zone, const int index_section, SectionTopology& section) const
{
    char section_name[CGNS_NAME_MAX_LEN] = { };
    int boundary_element_count = 0;
    int parent_flag = 0;
    if (CGNS_LOG_CALL(cg_section_read(this->get_file_id(),
                                      index_base,
                                      index_zone,
                                      index_section,
                                      section_name,
                                      &section.type,
                                      &section.range_start,
                                      &section.range_end,
                                      &boundary_element_count,
                                      &parent_flag)) != CG_OK) {
        return false;
    }

    section.index = index_section;
    section.name = section_name;
    section.has_parent_data = parent_flag != 0;

    // Parent data is intentionally not cached; the connectivity readers pass a null parent buffer.
    if (section.range_start < 1 || section.range_end < section.range_start) {
        LOG_ERROR("Invalid element range [{}, {}] at Base {}/Zone {}/Section {}.", section.range_start, section.range_end, index_base, index_zone, index_section);
        return false;
    }

    const cgsize_t element_count_value = section.range_end - section.range_start + 1;
    if (!std::in_range<std::size_t>(element_count_value)) {
        LOG_ERROR("Element count exceeds addressable memory at Base {}/Zone {}/Section {}.", index_base, index_zone, index_section);
        return false;
    }
    const std::size_t element_count = static_cast<std::size_t>(element_count_value);

    cgsize_t element_data_size = 0;
    if (CGNS_LOG_CALL(cg_ElementDataSize(this->get_file_id(), index_base, index_zone, index_section, &element_data_size)) != CG_OK || element_data_size < 0 ||
        !std::in_range<std::size_t>(element_data_size)) {
        return false;
    }

    if (IsVariableElementType(section.type)) {
        if (element_count == std::numeric_limits<std::size_t>::max()) {
            LOG_ERROR("Connectivity offsets exceed addressable memory at Base {}/Zone {}/Section {}.", index_base, index_zone, index_section);
            return false;
        }

        section.elements.resize(static_cast<std::size_t>(element_data_size));
        section.connect_offset.resize(element_count + 1);
        if (CGNS_LOG_CALL(cg_poly_elements_read(this->get_file_id(),
                                                index_base,
                                                index_zone,
                                                index_section,
                                                section.elements.data(),
                                                section.connect_offset.data(),
                                                nullptr)) != CG_OK) {
            return false;
        }
        if (section.connect_offset.front() != 0 || section.connect_offset.back() != element_data_size || !std::ranges::is_sorted(section.connect_offset)) {
            LOG_ERROR("Invalid connectivity offsets at Base {}/Zone {}/Section {}.", index_base, index_zone, index_section);
            return false;
        }
        return true;
    }

    int nodes_per_element = 0;
    if (CGNS_LOG_CALL(cg_npe(section.type, &nodes_per_element)) != CG_OK || nodes_per_element <= 0) {
        LOG_ERROR("Unsupported fixed element type [{}] at Base {}/Zone {}/Section {}.", cg_ElementTypeName(section.type), index_base, index_zone, index_section);
        return false;
    }

    const std::size_t node_count = static_cast<std::size_t>(nodes_per_element);
    if (element_count > std::numeric_limits<std::size_t>::max() / node_count || element_count * node_count != static_cast<std::size_t>(element_data_size)) {
        LOG_ERROR("Connectivity size does not match element type [{}] at Base {}/Zone {}/Section {}.",
                  cg_ElementTypeName(section.type),
                  index_base,
                  index_zone,
                  index_section);
        return false;
    }

    section.elements.resize(static_cast<std::size_t>(element_data_size));
    return CGNS_LOG_CALL(cg_elements_read(this->get_file_id(), index_base, index_zone, index_section, section.elements.data(), nullptr)) == CG_OK;
}

bool ReaderMeshData::build_structured_section(ZoneTopology& zone) const
{
    if (zone.type != CG_ZoneType_t::CG_Structured || zone.dim < 1 || zone.dim > 3) {
        LOG_ERROR("Cannot build a structured section for Zone {} with type [{}] and dimension {}.", zone.index, cg_ZoneTypeName(zone.type), zone.dim);
        return false;
    }

    const cgsize_t NVertexI = zone.zone_size[0];
    const cgsize_t NCellI = NVertexI;

    cgsize_t cell_sum = 0;

    SectionTopology section;
    if (zone.dim == 1) {
        cell_sum = zone.zone_size[1];
        section.elements.reserve(cell_sum * 2);
        section.type = CG_BAR_2;

        std::array<cgsize_t, 2> element_node { };
        for (cgsize_t i = 1; i < NCellI; ++i) {
            element_node[0] = i;
            element_node[1] = i + 1;

            section.elements.insert(section.elements.end(), element_node.begin(), element_node.end());
        }
    }
    else if (zone.dim == 2) {
        cell_sum = zone.zone_size[2] * zone.zone_size[3];
        section.elements.reserve(cell_sum * 4);
        section.type = CG_QUAD_4;

        const cgsize_t NVertexJ = zone.zone_size[1];
        const cgsize_t NCellJ = NVertexJ;

        auto vertex_id = [&NVertexI](cgsize_t i, cgsize_t j) {
            return (j - 1) * NVertexI + i;
        };

        std::array<cgsize_t, 4> element_node { };
        for (cgsize_t j = 1; j < NCellJ; ++j) {
            for (cgsize_t i = 1; i < NCellI; ++i) {
                element_node[0] = vertex_id(i, j);
                element_node[1] = vertex_id(i + 1, j);
                element_node[2] = vertex_id(i + 1, j + 1);
                element_node[3] = vertex_id(i, j + 1);

                section.elements.insert(section.elements.end(), element_node.begin(), element_node.end());
            }
        }
    }
    else {
        cell_sum = zone.zone_size[3] * zone.zone_size[4] * zone.zone_size[5];
        section.elements.reserve(cell_sum * 8);
        section.type = CG_HEXA_8;

        const cgsize_t NVertexJ = zone.zone_size[1];
        const cgsize_t NVertexK = zone.zone_size[2];

        const cgsize_t NCellJ = NVertexJ;
        const cgsize_t NCellK = NVertexK;

        auto vertex_id = [&NVertexI, &NVertexJ](cgsize_t i, cgsize_t j, cgsize_t k) {
            return (k - 1) * (NVertexI * NVertexJ) + (j - 1) * NVertexI + i;
        };

        std::array<cgsize_t, 8> element_node { };
        for (cgsize_t k = 1; k < NCellK; ++k) {
            for (cgsize_t j = 1; j < NCellJ; ++j) {
                for (cgsize_t i = 1; i < NCellI; ++i) {
                    element_node[0] = vertex_id(i, j, k);
                    element_node[1] = vertex_id(i + 1, j, k);
                    element_node[2] = vertex_id(i + 1, j + 1, k);
                    element_node[3] = vertex_id(i, j + 1, k);
                    element_node[4] = vertex_id(i, j, k + 1);
                    element_node[5] = vertex_id(i + 1, j, k + 1);
                    element_node[6] = vertex_id(i + 1, j + 1, k + 1);
                    element_node[7] = vertex_id(i, j + 1, k + 1);

                    section.elements.insert(section.elements.end(), element_node.begin(), element_node.end());
                }
            }
        }
    }

    section.index = 1;
    section.name = zone.name;
    section.range_start = 1;
    section.range_end = cell_sum;
    zone.sections.emplace_back(std::move(section));

    return true;
}
