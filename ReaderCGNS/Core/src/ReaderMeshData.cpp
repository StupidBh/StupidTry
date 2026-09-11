#include "ReaderMeshData.h"

#include <limits>
#include <source_location>

namespace {
    constexpr bool IsVariableElementType(const CG_ElementType_t element_type) noexcept
    {
        return element_type == CG_ElementType_t::CG_MIXED || element_type == CG_ElementType_t::CG_NGON_n || element_type == CG_ElementType_t::CG_NFACE_n;
    }
} // namespace

bool ReaderMeshData::GetAllNodeCoordinates(std::vector<ReaderAPI::Node>& node_coordinates)
{
    if (this->m_elements.empty()) {
        if (!this->initialize_grid_topology()) {
            return false;
        }
    }
    utils::AppendVector(node_coordinates, this->m_node_coordinates);

    if (node_coordinates.empty()) {
        LOG_WARN("Node coordinates is empty.");
    }
    return true;
}

bool ReaderMeshData::GetAllElement(std::vector<ReaderAPI::Elem>& elements)
{
    if (this->m_elements.empty()) {
        if (!this->initialize_grid_topology()) {
            return false;
        }
    }

    utils::AppendVector(elements, this->m_elements);
    return true;
}

bool ReaderMeshData::GetAllElementSetName(std::vector<std::string>& element_set_names)
{
    if (this->m_components.empty()) {
        if (!this->initialize_grid_topology()) {
            return false;
        }
    }

    std::vector<std::string> loaded_components;
    for (const auto& name : this->m_components | std::views::keys) {
        loaded_components.emplace_back(name);
    }

    utils::AppendVector(element_set_names, std::move(loaded_components));
    return true;
}

void ReaderMeshData::clear_grid_topology() noexcept
{
    utils::DeepClear(this->m_node_coordinates, this->m_elements, this->m_components);
}

bool ReaderMeshData::initialize_grid_topology()
{
    if (!this->IsOpen()) {
        LOG_ERROR("Cannot initialize grid topology without an open CGNS file.");
        return false;
    }
    m_node_offset = m_element_offset = 0;

    bool is_invalid = true;
    for (const auto& [base_index, zone_indices] : this->get_base_zone_indices()) {
        BaseTopology base;
        base.index = base_index;
        char base_name[CGNS_NAME_MAX_LEN] = { };
        if (CGNS_LOG_CALL(cg_base_read(this->get_file_id(), base.index, base_name, &base.cell_dim, &base.phy_dim)) != CG_OK) {
            return false;
        }
        CGNS_LOG_CALL(cg_simulation_type_read(this->get_file_id(), base.index, &base.type));

        base.name = base_name;
        LOG_INFO("Init [Base] [{}] {}, CellDim={}, PhyDim={}", cg_SimulationTypeName(base.type), base.name, base.cell_dim, base.phy_dim);

        for (const int zone_index : zone_indices) {
            ZoneTopology zone;
            zone.index = zone_index;
            if (!this->read_zone_topology(base, zone)) {
                continue;
            }

            const cgsize_t node_count = zone.NodeSum();
            if (node_count < 0 || node_count > std::numeric_limits<ReaderAPI::Integer>::max() - this->m_node_offset) {
                LOG_ERROR("Node count exceeds the ReaderAPI::Integer range.");
                return false;
            }

            if (!this->m_ngon_nface.empty()) {
                this->fatten_section_elem_poly(false);
            }

            for (std::size_t i = 0; i < node_count; ++i) {
                this->m_node_coordinates.emplace_back(ReaderAPI::Node { .id = static_cast<ReaderAPI::Integer>(this->m_node_offset + i),
                                                                        .x = zone.coordinates_xyz[0][i],
                                                                        .y = zone.coordinates_xyz[1][i],
                                                                        .z = zone.coordinates_xyz[2][i] });
            }
            this->m_node_offset += node_count;
        }

        if (is_invalid) {
            is_invalid = false;
        }
    }

    if (is_invalid) {
        LOG_ERROR("Failed to initialize grid topology: no base topology could be loaded.");
        return false;
    }

    return true;
}

void ReaderMeshData::update_components(std::string& component_name, std::size_t element_count, cgsize_t element_start)
{
    int component_count = 0;
    while (this->m_components.contains(component_name)) {
        component_name = std::format("{}_{}", component_name, component_count);
        ++component_count;
    }
    this->m_components[component_name] = utils::CreateVector<ReaderAPI::Integer>(element_count, element_start);
}

bool ReaderMeshData::read_zone_topology(const BaseTopology& base, ZoneTopology& zone)
{
    if (CGNS_LOG_CALL(cg_zone_type(this->get_file_id(), base.index, zone.index, &zone.type)) != CG_OK ||
        CGNS_LOG_CALL(cg_index_dim(this->get_file_id(), base.index, zone.index, &zone.dim)) != CG_OK) {
        return false;
    }

    if (zone.type == CG_ZoneType_t::CG_Structured) {
        if (zone.dim < 1 || zone.dim > 3) {
            LOG_ERROR("Invalid structured index dimension {} at Base {}/Zone {}.", zone.dim, base.index, zone.index);
            return false;
        }
    }

    char zone_name[CGNS_NAME_MAX_LEN] = { };
    if (CGNS_LOG_CALL(cg_zone_read(this->get_file_id(), base.index, zone.index, zone_name, zone.zone_size.data())) != CG_OK) {
        return false;
    }

    if (std::ranges::any_of(zone.zone_size, [](const cgsize_t value) { return value < 0; })) {
        LOG_ERROR("Invalid zone dimensions at Base {}/Zone {}.", base.index, zone.index);
        return false;
    }

    if (!read_zone_coordinates(base.index, zone)) {
        return false;
    }
    zone.name = zone_name;
    LOG_INFO("Init [Zone] [{}] {}, Dim={}, Size={}", cg_ZoneTypeName(zone.type), zone.name, zone.dim, std::span(zone.zone_size).subspan(0, zone.dim * 3));

    if (zone.type == CG_ZoneType_t::CG_Structured) {
        auto element_init = this->m_elements.size();
        auto element_begin = this->m_element_offset;
        if (this->build_structured_section(zone)) {
            std::string component_name = std::format("{}.{}", base.name, zone.name);
            this->update_components(component_name, (this->m_elements.size() - element_init), element_begin);
            return true;
        }
    }

    return this->read_unstructured_zone_sections(base, zone);
}

bool ReaderMeshData::read_zone_coordinates(int index_base, ZoneTopology& zone) const
{
    int zone_ncoords = 0;
    if (CGNS_LOG_CALL(cg_ncoords(this->get_file_id(), index_base, zone.index, &zone_ncoords)) != CG_OK) {
        return false;
    }
    if (zone_ncoords < 1 || zone_ncoords > 3) {
        LOG_ERROR("Invalid zone ncoords {} at Base {}/Zone {}.", zone_ncoords, index_base, zone.index);
        return false;
    }

    const std::vector<cgsize_t> r_min(zone.dim, 1);
    std::vector<cgsize_t> r_max(zone.dim, 1);
    for (int i = 0; i < zone.dim; ++i) {
        r_max[i] = zone.zone_size[i];
    }
    zone.coordinates_xyz.fill(std::vector(zone.NodeSum(), 0.F));

    for (int index_coord = 1; index_coord <= zone_ncoords; ++index_coord) {
        char index_coord_name[CGNS_NAME_MAX_LEN] = { };
        CG_DataType_t index_coord_type = CG_DataType_t::CG_DataTypeNull;
        if (CGNS_LOG_CALL(cg_coord_info(this->get_file_id(), index_base, zone.index, index_coord, &index_coord_type, index_coord_name)) != CG_OK) {
            zone.coordinates_xyz.fill(std::vector(0, 0.F));
            break;
        }

        if (index_coord_type == CG_DataType_t::CG_RealSingle) {
            CGNS_LOG_CALL(cg_coord_read(this->get_file_id(),
                                        index_base,
                                        zone.index,
                                        index_coord_name,
                                        index_coord_type,
                                        r_min.data(),
                                        r_max.data(),
                                        zone.coordinates_xyz[index_coord - 1].data()));
        }
        else if (index_coord_type == CG_DataType_t::CG_RealDouble) {
            std::vector<double> temp_buff(zone.NodeSum(), 0.0);

            CGNS_LOG_CALL(
                cg_coord_read(this->get_file_id(), index_base, zone.index, index_coord_name, index_coord_type, r_min.data(), r_max.data(), temp_buff.data()));
            zone.coordinates_xyz[index_coord - 1] = utils::ShrinkVector<float>(temp_buff);
        }
        else {
            LOG_WARN("[ZoneCoords]{:>2}:[{}] {}, unsupported type at Base {}/Zone {}.",
                     index_coord,
                     cg_DataTypeName(index_coord_type),
                     index_coord_name,
                     index_base,
                     zone.index);
            utils::DeepClear(zone.coordinates_xyz[index_coord - 1]);
        }
    }

    if (zone.coordinates_xyz[0].empty() || zone.coordinates_xyz[1].empty() || zone.coordinates_xyz[2].empty()) {
        LOG_ERROR("Read coordinates failed at Base {}/Zone {}.", index_base, zone.index);
        return false;
    }

    return true;
}

bool ReaderMeshData::read_unstructured_zone_sections(const BaseTopology& base, ZoneTopology& zone)
{
    int section_count = 0;
    if (CGNS_LOG_CALL(cg_nsections(this->get_file_id(), base.index, zone.index, &section_count)) != CG_OK) {
        return false;
    }
    if (section_count < 0) {
        LOG_ERROR("Invalid section count {} at Base {}/Zone {}.", section_count, base.index, zone.index);
        return false;
    }

    std::vector<SectionTopology> section_topologies;
    section_topologies.reserve(static_cast<std::size_t>(section_count));
    for (int index_section = 1; index_section <= section_count; ++index_section) {
        SectionTopology section;
        section.index = index_section;
        if (!this->read_section_topology(base, zone, section)) {
            continue;
        }
        section_topologies.emplace_back(std::move(section));
    }

    return !section_topologies.empty();
}

bool ReaderMeshData::read_section_topology(const BaseTopology& base, const ZoneTopology& zone, SectionTopology& section)
{
    char section_name[CGNS_NAME_MAX_LEN] = { };
    int boundary_element_count = 0;
    int parent_flag = 0;
    if (CGNS_LOG_CALL(cg_section_read(this->get_file_id(),
                                      base.index,
                                      zone.index,
                                      section.index,
                                      section_name,
                                      &section.type,
                                      &section.range_start,
                                      &section.range_end,
                                      &boundary_element_count,
                                      &parent_flag)) != CG_OK) {
        return false;
    }

    section.name = section_name;
    section.has_parent_data = parent_flag != 0;

    // Parent data is intentionally not cached; the connectivity readers pass a null parent buffer.
    if (section.range_start < 1 || section.range_end < section.range_start) {
        LOG_ERROR("Invalid element range [{}, {}] at Base {}/Zone {}/Section {}.", section.range_start, section.range_end, base.index, zone.index, section.index);
        return false;
    }

    const cgsize_t element_count_value = section.range_end - section.range_start + 1;
    if (!std::in_range<std::size_t>(element_count_value)) {
        LOG_ERROR("Element count exceeds addressable memory at Base {}/Zone {}/Section {}.", base.index, zone.index, section.index);
        return false;
    }
    const std::size_t element_count = static_cast<std::size_t>(element_count_value);

    cgsize_t element_data_size = 0;
    if (CGNS_LOG_CALL(cg_ElementDataSize(this->get_file_id(), base.index, zone.index, section.index, &element_data_size)) != CG_OK) {
        return false;
    }

    if (element_data_size < 0 || !std::in_range<std::size_t>(element_data_size)) {
        LOG_ERROR("Element data size value is invalid [{}] at Base {}/Zone {}/Section {}.", element_data_size, base.index, zone.index, section.index);
        return false;
    }

    if (IsVariableElementType(section.type)) {
        if (element_count == std::numeric_limits<std::size_t>::max()) {
            LOG_ERROR("Connectivity offsets exceed addressable memory at Base {}/Zone {}/Section {}.", base.index, zone.index, section.index);
            return false;
        }

        section.elements.resize(static_cast<std::size_t>(element_data_size));
        section.connect_offset.resize(element_count + 1);
        if (CGNS_LOG_CALL(cg_poly_elements_read(this->get_file_id(),
                                                base.index,
                                                zone.index,
                                                section.index,
                                                section.elements.data(),
                                                section.connect_offset.data(),
                                                nullptr)) != CG_OK) {
            return false;
        }
        if (section.connect_offset.front() != 0 || section.connect_offset.back() != element_data_size || !std::ranges::is_sorted(section.connect_offset)) {
            LOG_ERROR("Invalid connectivity offsets at Base {}/Zone {}/Section {}.", base.index, zone.index, section.index);
            return false;
        }
        if (section.type == CG_MIXED) {
            auto element_init = this->m_elements.size();
            auto element_begin = this->m_element_offset;

            if (this->fatten_section_elem_mixed(section)) {
                std::string component_name = std::format("{}.{}.{}", base.name, zone.name, section.name);
                this->update_components(component_name, (this->m_elements.size() - element_init), element_begin);
                return true;
            }
        }
        else {
            section.name = std::format("{}.{}.{}", base.name, zone.name, section_name);
            this->m_ngon_nface.emplace_back(section);
        }
        return false;
    }

    int nodes_per_element = 0;
    if (CGNS_LOG_CALL(cg_npe(section.type, &nodes_per_element)) != CG_OK || nodes_per_element <= 0) {
        LOG_ERROR("Unsupported fixed element type [{}] at Base {}/Zone {}/Section {}.", cg_ElementTypeName(section.type), base.index, zone.index, section.index);
        return false;
    }

    const std::size_t node_count = static_cast<std::size_t>(nodes_per_element);
    if (element_count > std::numeric_limits<std::size_t>::max() / node_count || element_count * node_count != static_cast<std::size_t>(element_data_size)) {
        LOG_ERROR("Connectivity size does not match element type [{}] at Base {}/Zone {}/Section {}.",
                  cg_ElementTypeName(section.type),
                  base.index,
                  zone.index,
                  section.index);
        return false;
    }

    section.elements.resize(static_cast<std::size_t>(element_data_size));
    if (CGNS_LOG_CALL(cg_elements_read(this->get_file_id(), base.index, zone.index, section.index, section.elements.data(), nullptr)) != CG_OK) {
        return false;
    }

    auto element_init = this->m_elements.size();
    auto element_begin = this->m_element_offset;
    if (!this->fatten_section_elem_normal(section)) {
        return false;
    }
    std::string component_name = std::format("{}.{}.{}", base.name, zone.name, section.name);
    this->update_components(component_name, (this->m_elements.size() - element_init), element_begin);

    return true;
}

bool ReaderMeshData::build_structured_section(ZoneTopology& zone)
{
    if (zone.type != CG_ZoneType_t::CG_Structured || zone.dim < 1 || zone.dim > 3) {
        LOG_ERROR("Cannot build a structured section for Zone {} with type [{}] and dimension {}.", zone.index, cg_ZoneTypeName(zone.type), zone.dim);
        return false;
    }

    const cgsize_t NVertexI = zone.zone_size[0];
    const cgsize_t NCellI = NVertexI;

    SectionTopology section;
    if (zone.dim == 1) {
        section.elements.reserve(zone.CellSum() * 2);
        section.type = CG_BAR_2;

        std::array<cgsize_t, 2> element_node { };
        for (cgsize_t i = 1; i < NCellI; ++i) {
            element_node[0] = i;
            element_node[1] = i + 1;

            section.elements.insert(section.elements.end(), element_node.begin(), element_node.end());
        }
    }
    else if (zone.dim == 2) {
        section.elements.reserve(zone.CellSum() * 4);
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
        section.elements.reserve(zone.CellSum() * 8);
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
    section.range_end = zone.CellSum();
    if (!this->fatten_section_elem_normal(section)) {
        return false;
    }

    return true;
}

bool ReaderMeshData::fatten_section_elem_normal(const SectionTopology& section)
{
    const auto element_sum = section.ElemSum();
    if (element_sum < 1 || element_sum > static_cast<cgsize_t>(std::numeric_limits<ReaderAPI::Integer>::max() - this->m_element_offset)) {
        LOG_ERROR("Element count in section [{}] {} exceeds the <ReaderAPI::Integer> range.", cg_ElementTypeName(section.type), section.name);
        return false;
    }

    LOG_INFO("Init section [{}] {}, range=[{}, {}]:{}",
             cg_ElementTypeName(section.type),
             section.name,
             section.range_start,
             section.range_end,
             section.elements.size());
    int npts = 0;
    if (CGNS_LOG_CALL(cg_npe(section.type, &npts)) != CG_OK) {
        return false;
    }
    if (npts < 1) {
        LOG_ERROR("Invalid element points {} in type {}.", npts, cg_ElementTypeName(section.type));
        return false;
    }

    auto& element_nodes = section.elements;
    std::vector<ReaderAPI::Elem> loaded_elements;
    loaded_elements.reserve(element_sum);
    for (std::size_t i = 0; i < element_nodes.size(); i += npts) {
        std::vector<ReaderAPI::Integer> element_node;
        element_node.reserve(npts);
        for (std::size_t j = i; j < i + npts && j < element_nodes.size(); ++j) {
            element_node.emplace_back(static_cast<ReaderAPI::Integer>(element_nodes[j] + this->m_node_offset - 1));
        }

        if (element_node.size() != npts) {
            continue;
        }

        loaded_elements.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(this->m_element_offset + section.range_start + loaded_elements.size()),
                                                       .type = static_cast<ReaderAPI::Integer>(section.type),
                                                       .npts = static_cast<ReaderAPI::Integer>(npts),
                                                       .nodes = std::move(element_node) });
        ++this->m_element_offset;
    }

    if (loaded_elements.empty()) {
        LOG_WARN("Section [{}] {}, valid element is empty.", cg_ElementTypeName(section.type), section.name);
        return false;
    }
    utils::AppendVector(this->m_elements, std::move(loaded_elements));
    return true;
}

bool ReaderMeshData::fatten_section_elem_mixed(const SectionTopology& section)
{
    const auto element_sum = section.ElemSum();
    if (element_sum < 1 || element_sum > static_cast<cgsize_t>(std::numeric_limits<ReaderAPI::Integer>::max() - this->m_element_offset)) {
        LOG_ERROR("Element count in section [MIXED] {} exceeds the <ReaderAPI::Integer> range.", section.name);
        return false;
    }

    auto& element_nodes = section.elements;
    auto& connect_offset = section.connect_offset;
    LOG_INFO("Init section [MIXED]-[{}] {}, range=[{}, {}]:{}",
             ElementTypeName[element_nodes[0]],
             section.name,
             section.range_start,
             section.range_end,
             section.elements.size());

    std::vector<ReaderAPI::Elem> loaded_elements;
    loaded_elements.reserve(element_sum);
    for (std::size_t i = 0; i < connect_offset.size() - 1; ++i) {
        cgsize_t element_node_begin = connect_offset[i];
        if (element_node_begin >= element_nodes.size()) {
            LOG_WARN("Invalid index: {} in elements range=[0, {}].", element_node_begin, element_nodes.size());
            continue;
        }

        cgsize_t element_type_id = element_nodes[element_node_begin];
        if (element_type_id < CG_ElementType_t::CG_NODE || element_type_id >= NofValidElementTypes) {
            LOG_ERROR("Invalid element type id: {}, falling back to [{}].", element_type_id, cg_ElementTypeName(CG_ElementType_t::CG_NODE));
            element_type_id = static_cast<cgsize_t>(CG_ElementType_t::CG_NODE);
        }

        const cgsize_t element_node_end = connect_offset[i + 1];
        const cgsize_t npts = element_node_end - element_node_begin - 1;
        if (npts < 1) {
            continue;
        }

        std::vector<ReaderAPI::Integer> element_node;
        element_node.reserve(npts);
        for (std::size_t j = element_node_begin + 1; j < element_node_end && j < element_nodes.size(); ++j) {
            element_node.emplace_back(static_cast<ReaderAPI::Integer>(element_nodes[j] + this->m_node_offset - 1));
        }

        if (element_node.size() != npts) {
            continue;
        }

        loaded_elements.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(this->m_element_offset + section.range_start + loaded_elements.size()),
                                                       .type = static_cast<ReaderAPI::Integer>(element_type_id),
                                                       .npts = static_cast<ReaderAPI::Integer>(npts),
                                                       .nodes = std::move(element_node) });
        ++this->m_element_offset;
    }

    if (loaded_elements.empty()) {
        LOG_WARN("Section [MIXED] {}, valid element is empty.", section.name);
        return false;
    }
    utils::AppendVector(this->m_elements, std::move(loaded_elements));
    return true;
}

void ReaderMeshData::fatten_section_elem_poly(bool separate_surface)
{
    auto get_elems = [this](const SectionTopology& section, const cgsize_t offset, const cgsize_t node) -> std::vector<ReaderAPI::Elem> {
        std::vector<ReaderAPI::Elem> elems;

        auto& element_nodes = section.elements;
        auto& connect_offset = section.connect_offset;

        LOG_INFO("Init section [{}] {}, size={}", cg_ElementTypeName(section.type), section.name, section.ElemSum());
        for (std::size_t i = 1; i < connect_offset.size(); ++i) {
            cgsize_t npts = connect_offset[i] - connect_offset[i - 1];

            std::vector<ReaderAPI::Integer> element_node;
            element_node.reserve(npts);
            for (std::size_t j = connect_offset[i - 1]; j < connect_offset[i] && j < element_nodes.size(); ++j) {
                element_node.emplace_back(static_cast<ReaderAPI::Integer>(element_nodes[j] + node));
            }
            if (element_node.empty()) {
                continue;
            }
            elems.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(offset + section.range_start + elems.size()),
                                                 .type = static_cast<ReaderAPI::Integer>(section.type),
                                                 .npts = static_cast<ReaderAPI::Integer>(npts),
                                                 .nodes = std::move(element_node) });
        }

        return elems;
    };

    std::vector<ReaderAPI::Elem> face_elements;
    std::vector<std::pair<std::string, std::vector<ReaderAPI::Elem>>> nface_sections;

    for (auto& section : this->m_ngon_nface) {
        if (section.type == CG_ElementType_t::CG_NGON_n) {
            std::vector<ReaderAPI::Elem> temp_elems = get_elems(section, this->m_element_offset, this->m_node_offset - 1);

            if (temp_elems.empty()) {
                LOG_WARN("Section [{}] {} is empty.", cg_ElementTypeName(section.type), section.name);
            }
            else {
                if (separate_surface) { // 将 NGON 也视为 component
                    this->update_components(section.name, temp_elems.size(), this->m_element_offset);
                    this->m_element_offset += temp_elems.size();
                }
                face_elements.insert(face_elements.end(), temp_elems.begin(), temp_elems.end());
            }
        }
        else if (section.type == CG_ElementType_t::CG_NFACE_n) {
            std::vector<ReaderAPI::Elem> temp_elems = get_elems(section, this->m_element_offset, 0);

            if (temp_elems.empty()) {
                LOG_WARN("Section [{}] {} is empty.", cg_ElementTypeName(section.type), section.name);
            }
            else {
                this->m_element_offset += temp_elems.size();
                nface_sections.emplace_back(section.name.substr(0, section.name.rfind(".")), std::move(temp_elems));
            }
        }

        else {
            LOG_WARN("Section {}, Invalid type [{}].", section.name, cg_ElementTypeName(section.type));
        }
    }

    for (auto& [component_name, nfaces] : nface_sections) {
        const auto element_start = this->m_elements.size();
        for (auto& nface : nfaces) {
            std::vector<ReaderAPI::Integer> element_node;
            for (auto& nface_node : nface.nodes) {
                auto face_index = std::abs(nface_node) - 1;
                if (face_index < face_elements.size()) {
                    auto& face = face_elements[face_index];
                    element_node.emplace_back(face.npts);

                    if (nface_node < 0) {
                        std::ranges::reverse(face.nodes);
                    }
                    utils::AppendVector(element_node, face.nodes);
                }
                else {
                    LOG_WARN("Invalid face id.");
                    nface.npts--;
                }
            }

            if (nface.npts > 0) {
                nface.nodes = std::move(element_node);
                this->m_elements.emplace_back(nface);
            }
        }

        if (this->m_elements.size() > element_start) {
            this->update_components(component_name, this->m_elements.size() - element_start, static_cast<cgsize_t>(element_start));
        }
    }

    this->m_ngon_nface.clear();
}
