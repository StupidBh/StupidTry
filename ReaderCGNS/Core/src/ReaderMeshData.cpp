#include "ReaderMeshData.h"

#include <limits>
#include <source_location>

#include "Utils/Utils.hpp"

namespace {
    constexpr bool IsVariableElementType(const CG_ElementType_t element_type) noexcept
    {
        return element_type == CG_ElementType_t::CG_MIXED || element_type == CG_ElementType_t::CG_NGON_n || element_type == CG_ElementType_t::CG_NFACE_n;
    }
} // namespace

bool ReaderMeshData::GetAllNodeCoordinates(std::vector<ReaderAPI::Node>& node_coordinates)
{
    if (this->m_grid_topology.empty()) {
        if (!this->initialize_grid_topology()) {
            return false;
        }
    }

    cgsize_t node_offset = 0;
    for (const auto& grid_topology : this->m_grid_topology) {
        for (auto& zone_topology : grid_topology.zones) {
            const cgsize_t node_count = zone_topology.NodeSum();
            if (node_count < 0 || node_count > std::numeric_limits<ReaderAPI::Integer>::max() - node_offset) {
                LOG_ERROR("Node count exceeds the ReaderAPI::Integer range.");
                return false;
            }

            for (std::size_t i = 0; i < node_count; ++i) {
                node_coordinates.emplace_back(ReaderAPI::Node { .id = static_cast<ReaderAPI::Integer>(node_offset + i),
                                                                .x = zone_topology.coordinates_xyz[0][i],
                                                                .y = zone_topology.coordinates_xyz[1][i],
                                                                .z = zone_topology.coordinates_xyz[2][i] });
            }
            node_offset += node_count;
        }
    }

    if (node_coordinates.empty()) {
        LOG_WARN("Node coordinates is empty.");
    }
    return true;
}

bool ReaderMeshData::GetAllElement(std::vector<ReaderAPI::Elem>& elements)
{
    if (this->m_elements.empty()) {
        if (!this->build_connectivity_components()) {
            return false;
        }
    }

    utils::AppendVector(elements, this->m_elements);
    return true;
}

bool ReaderMeshData::GetAllElementSetName(std::vector<std::string>& element_set_names)
{
    if (this->m_components.empty()) {
        if (!this->build_connectivity_components()) {
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
    utils::DeepClear(this->m_grid_topology, this->m_elements, this->m_components);
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

    if (loaded_topology.empty()) {
        LOG_ERROR("Failed to initialize grid topology: no base topology could be loaded.");
        return false;
    }

    this->m_grid_topology = std::move(loaded_topology);
    return !this->m_grid_topology.empty();
}

bool ReaderMeshData::build_connectivity_components()
{
    if (this->m_grid_topology.empty()) {
        if (!this->initialize_grid_topology()) {
            return false;
        }
    }
    cgsize_t node_offset = 0;
    cgsize_t element_offset = 0;

    std::vector<ReaderAPI::Elem> loaded_elements;
    for (const auto& base_topology : this->m_grid_topology) {
        LOG_INFO("Init Base [{}] {}, CellDim={}, PhyDim={}",
                 cg_SimulationTypeName(base_topology.type),
                 base_topology.name,
                 base_topology.cell_dim,
                 base_topology.phy_dim);

        for (auto& zone_topology : base_topology.zones) {
            LOG_INFO("Init Zone [{}] {}, NodeSum={}, CellSum={}",
                     cg_ZoneTypeName(zone_topology.type),
                     zone_topology.name,
                     zone_topology.NodeSum(),
                     zone_topology.CellSum());

            for (auto& section_topology : zone_topology.sections) {
                if (section_topology.type == CG_ElementType_t::CG_NGON_n || section_topology.type == CG_ElementType_t::CG_NFACE_n) {
                    bool flag = false;
                    {
                        int nsols = 0;
                        if (CGNS_LOG_CALL(cg_nsols(this->get_file_id(), base_topology.index, zone_topology.index, &nsols)) == CG_OK) {
                            if (nsols > 0) {
                                char solution_name[CGNS_NAME_MAX_LEN] = { };
                                CG_GridLocation_t solution_location = CG_GridLocation_t::CG_GridLocationNull;
                                if (CGNS_LOG_CALL(
                                        cg_sol_info(this->get_file_id(), base_topology.index, section_topology.index, 1, solution_name, &solution_location)) ==
                                    CG_OK) {
                                    flag = solution_location == CG_GridLocation_t::CG_Vertex;
                                }
                            }
                        }
                    }

                    this->initialize_section_ngon_nface(base_topology.name, zone_topology, loaded_elements, element_offset, node_offset, flag);
                    break;
                }

                cgsize_t element_count = 0;
                if (section_topology.type == CG_ElementType_t::CG_MIXED) {
                    element_count = this->initialize_section_mixed(section_topology, loaded_elements, element_offset, node_offset);
                }
                else {
                    element_count = this->initialize_section_normal(section_topology, loaded_elements, element_offset, node_offset);
                }

                if (element_count == 0) {
                    continue;
                }

                auto component_name = std::format("{}.{}.{}", base_topology.name, zone_topology.name, section_topology.name);
                this->update_components(component_name, element_count, element_offset);

                element_offset += element_count;
            }

            node_offset += zone_topology.NodeSum();
        }
    }

    if (loaded_elements.empty() || this->m_components.empty()) {
        LOG_WARN("Element is empty.");
        return false;
    }
    m_elements = std::move(loaded_elements);
    return true;
}

cgsize_t ReaderMeshData::initialize_section_mixed(const SectionTopology& section,
                                                  std::vector<ReaderAPI::Elem>& elements,
                                                  const cgsize_t& element_offset,
                                                  const cgsize_t& node_offset) const
{
    if (section.elements.empty() || section.connect_offset.size() < 2) {
        LOG_ERROR("Init section [MIXED] {} failed: connectivity is empty.", section.name);
        return 0;
    }

    const cgsize_t element_count = section.ElemSum();
    if (element_count < 1 || element_count > static_cast<cgsize_t>(std::numeric_limits<ReaderAPI::Integer>::max() - element_offset)) {
        LOG_ERROR("Element count in section [MIXED] {} exceeds the ReaderAPI::Integer range.", section.name);
        return 0;
    }

    LOG_INFO("Init section [MIXED] {}, size={}", section.name, element_count);

    std::vector<ReaderAPI::Elem> loaded_elements;
    loaded_elements.reserve(static_cast<std::size_t>(element_count));
    const auto& connect_offset = section.connect_offset;
    for (std::size_t i = 0; i < connect_offset.size() - 1; ++i) {
        cgsize_t element_node_begin = connect_offset[i];
        cgsize_t elem_type_id = section.elements[element_node_begin];
        if (elem_type_id < CG_ElementType_t::CG_NODE || elem_type_id >= NofValidElementTypes) {
            LOG_ERROR("Invalid element type id: {}, falling back to [{}].", elem_type_id, cg_ElementTypeName(CG_ElementType_t::CG_NODE));
            elem_type_id = static_cast<cgsize_t>(CG_ElementType_t::CG_NODE);
        }

        int element_points = 0;
        if (CGNS_LOG_CALL(cg_npe(static_cast<CG_ElementType_t>(elem_type_id), &element_points)) != CG_OK) {
            continue;
        }
        if (element_points < 1 || element_points != connect_offset[i + 1] - connect_offset[i] - 1) {
            LOG_ERROR("Invalid element points {} in type [MIXED]-[{}].", element_points, ElementTypeName[elem_type_id]);
            continue;
        }

        std::vector<ReaderAPI::Integer> element_node;
        element_node.reserve(static_cast<std::size_t>(element_points));
        for (std::size_t j = element_node_begin + 1; j < connect_offset[i + 1] && j < section.elements.size(); ++j) {
            element_node.emplace_back(node_offset + static_cast<ReaderAPI::Integer>(section.elements[j] - 1));
        }

        loaded_elements.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(element_offset + loaded_elements.size()),
                                                       .type = static_cast<ReaderAPI::Integer>(elem_type_id),
                                                       .npts = static_cast<ReaderAPI::Integer>(element_node.size()),
                                                       .nodes = std::move(element_node) });
    }

    if (loaded_elements.empty()) {
        LOG_ERROR("Init section [MIXED] {} failed: element is empty.", section.name);
        return 0;
    }

    const auto loaded_element_count = static_cast<cgsize_t>(loaded_elements.size());
    utils::AppendVector(elements, std::move(loaded_elements));
    return loaded_element_count;
}

cgsize_t ReaderMeshData::initialize_section_normal(const SectionTopology& section,
                                                   std::vector<ReaderAPI::Elem>& elements,
                                                   const cgsize_t& element_offset,
                                                   const cgsize_t& node_offset) const
{
    const cgsize_t element_count = section.ElemSum();
    if (element_count < 1 || element_count > static_cast<cgsize_t>(std::numeric_limits<ReaderAPI::Integer>::max() - element_offset)) {
        LOG_ERROR("Element count in section [{}] {} exceeds the ReaderAPI::Integer range.", cg_ElementTypeName(section.type), section.name);
        return 0;
    }

    LOG_INFO("Init section [{}] {}, size={}", cg_ElementTypeName(section.type), section.name, section.ElemSum());
    int element_points = 0;
    if (CGNS_LOG_CALL(cg_npe(section.type, &element_points)) != CG_OK) {
        return 0;
    }
    if (element_points < 1) {
        LOG_ERROR("Invalid element points {} in type {}.", element_points, cg_ElementTypeName(section.type));
        return 0;
    }

    std::vector<ReaderAPI::Elem> loaded_elements;
    loaded_elements.reserve(static_cast<std::size_t>(element_count));
    for (std::size_t i = 0; i < section.elements.size(); i += element_points) {
        std::vector<ReaderAPI::Integer> element_node;
        element_node.reserve(static_cast<std::size_t>(element_points));
        for (std::size_t j = i; j < i + element_points; ++j) {
            element_node.emplace_back(node_offset + static_cast<ReaderAPI::Integer>(section.elements[j] - 1));
        }

        loaded_elements.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(element_offset + loaded_elements.size()),
                                                       .type = static_cast<ReaderAPI::Integer>(section.type),
                                                       .npts = static_cast<ReaderAPI::Integer>(element_points),
                                                       .nodes = std::move(element_node) });
    }

    if (loaded_elements.empty()) {
        LOG_ERROR("Init section [{}] {} failed: element is empty.", cg_ElementTypeName(section.type), section.name);
        return 0;
    }

    const auto loaded_element_count = static_cast<cgsize_t>(loaded_elements.size());
    utils::AppendVector(elements, std::move(loaded_elements));
    return loaded_element_count;
}

cgsize_t ReaderMeshData::initialize_section_ngon_nface(std::string_view base_name,
                                                       const ZoneTopology& zone_topology,
                                                       std::vector<ReaderAPI::Elem>& elements,
                                                       cgsize_t& element_offset,
                                                       cgsize_t node_offset,
                                                       bool flag)
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
                element_node.emplace_back(node + static_cast<ReaderAPI::Integer>(element_nodes[j]));
            }
            if (element_node.empty()) {
                continue;
            }
            elems.emplace_back(ReaderAPI::Elem { .id = static_cast<ReaderAPI::Integer>(offset + elems.size()),
                                                 .type = static_cast<ReaderAPI::Integer>(section.type),
                                                 .npts = static_cast<ReaderAPI::Integer>(npts),
                                                 .nodes = std::move(element_node) });
        }

        return elems;
    };

    std::vector<ReaderAPI::Elem> face_elements;
    std::vector<ReaderAPI::Elem> nface_elements;

    const auto initial_size = elements.size();
    std::vector<std::string> nface_component_names;
    std::vector<ReaderAPI::Integer> failed_nface_ids;

    for (auto& section : zone_topology.sections) {
        if (section.type == CG_ElementType_t::CG_NGON_n) {
            std::vector<ReaderAPI::Elem> temp_elems = get_elems(section, element_offset, node_offset - 1);

            if (temp_elems.empty()) {
                LOG_WARN("Section [{}] {} is empty.", cg_ElementTypeName(section.type), section.name);
            }
            else {
                if (flag) { // 将 NGON 也视为 component
                    auto component_name = std::format("{}.{}.{}", base_name, zone_topology.name, section.name);
                    this->update_components(component_name, temp_elems.size(), element_offset);

                    element_offset += temp_elems.size();
                }

                face_elements.insert(face_elements.end(), temp_elems.begin(), temp_elems.end());
            }
        }
        else if (section.type == CG_ElementType_t::CG_NFACE_n) {
            std::vector<ReaderAPI::Elem> temp_elems = get_elems(section, element_offset, 0);

            if (temp_elems.empty()) {
                LOG_WARN("Section [{}] {} is empty.", cg_ElementTypeName(section.type), section.name);
            }
            else {
                auto component_name = std::format("{}.{}.{}", base_name, zone_topology.name, section.name);
                this->update_components(component_name, temp_elems.size(), element_offset);
                nface_component_names.emplace_back(component_name);

                element_offset += temp_elems.size();
                nface_elements.insert(nface_elements.end(), temp_elems.begin(), temp_elems.end());
                utils::AppendVector(elements, std::move(face_elements));
            }
        }

        else {
            LOG_WARN("Section {}, Invalid type [{}].", section.name, cg_ElementTypeName(section.type));
        }
    }

    for (auto& nface : nface_elements) {
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
            elements.emplace_back(nface);
        }
        else {
            failed_nface_ids.emplace_back(nface.id);
        }
    }

    std::ranges::sort(failed_nface_ids);
    for (const auto& component_name : nface_component_names) {
        auto& ids = this->m_components.at(component_name);
        std::erase_if(ids, [&failed_nface_ids](ReaderAPI::Integer id) { return std::ranges::binary_search(failed_nface_ids, id); });
    }

    return static_cast<cgsize_t>(elements.size() - initial_size);
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
    return this->read_unstructured_zone_sections(index_base, index_zone, zone);
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
    for (int i = 0; i < zone.dim; ++i) {
        r_max[i] = zone.zone_size[i];
    }
    zone.coordinates_xyz.fill(std::vector(zone.NodeSum(), 0.F));

    for (int index_coord = 1; index_coord <= zone_ncoords; ++index_coord) {
        char index_coord_name[CGNS_NAME_MAX_LEN] = { };
        CG_DataType_t index_coord_type = CG_DataType_t::CG_DataTypeNull;
        if (CGNS_LOG_CALL(cg_coord_info(this->get_file_id(), index_base, index_zone, index_coord, &index_coord_type, index_coord_name)) != CG_OK) {
            zone.coordinates_xyz.fill(std::vector(0, 0.F));
            break;
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
            std::vector<double> temp_buff(zone.NodeSum(), 0.0);

            CGNS_LOG_CALL(
                cg_coord_read(this->get_file_id(), index_base, index_zone, index_coord_name, index_coord_type, r_min.data(), r_max.data(), temp_buff.data()));
            zone.coordinates_xyz[index_coord - 1] = utils::ShrinkVector<float>(temp_buff);
        }
        else {
            LOG_WARN("[ZoneCoords]{:>2}:[{}] {}, unsupported type at Base {}/Zone {}.",
                     index_coord,
                     cg_DataTypeName(index_coord_type),
                     index_coord_name,
                     index_base,
                     index_zone);
            utils::DeepClear(zone.coordinates_xyz[index_coord - 1]);
        }
    }

    if (zone.coordinates_xyz[0].empty() || zone.coordinates_xyz[1].empty() || zone.coordinates_xyz[2].empty()) {
        LOG_ERROR("Read coordinates failed at Base {}/Zone {}.", index_base, index_zone);
        return false;
    }
    return true;
}

bool ReaderMeshData::read_unstructured_zone_sections(const int index_base, const int index_zone, ZoneTopology& zone) const
{
    int section_count = 0;
    if (CGNS_LOG_CALL(cg_nsections(this->get_file_id(), index_base, index_zone, &section_count)) != CG_OK) {
        return false;
    }
    if (section_count < 0) {
        LOG_ERROR("Invalid section count {} at Base {}/Zone {}.", section_count, index_base, index_zone);
        return false;
    }

    zone.sections.reserve(static_cast<std::size_t>(section_count));
    for (int index_section = 1; index_section <= section_count; ++index_section) {
        SectionTopology section;
        if (!this->read_section_topology(index_base, index_zone, index_section, section)) {
            continue;
        }
        zone.sections.emplace_back(std::move(section));
    }

    return !zone.sections.empty();
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
    if (CGNS_LOG_CALL(cg_ElementDataSize(this->get_file_id(), index_base, index_zone, index_section, &element_data_size)) != CG_OK) {
        return false;
    }

    if (element_data_size < 0 || !std::in_range<std::size_t>(element_data_size)) {
        LOG_ERROR("Element data size value is invalid [{}] at Base {}/Zone {}/Section {}.", element_data_size, index_base, index_zone, index_section);
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
    zone.sections.emplace_back(std::move(section));

    return true;
}
