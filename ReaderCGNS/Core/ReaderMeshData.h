#pragma once
#include "FileManager.h"
#include "CgnsTopology.hpp"

class ReaderMeshData : virtual public FileManager {
public:
    ReaderMeshData() = default;
    ~ReaderMeshData() override = default;

    bool GetAllNodeCoordinates(std::vector<ReaderAPI::Node>& node_coordinates) final;
    bool GetAllElement(std::vector<ReaderAPI::Elem>& elements) final;

    bool GetAllElementSetName(std::vector<std::string>& element_set_names) final;

protected:
    void clear_grid_topology() noexcept;

    [[nodiscard]] bool initialize_grid_topology();

private:
    void update_components(std::string& component_name, std::size_t element_count, cgsize_t element_start);

    [[nodiscard]] bool read_zone_topology(const BaseTopology& base, ZoneTopology& zone);
    [[nodiscard]] bool read_zone_coordinates(int index_base, ZoneTopology& zone) const;
    bool read_unstructured_zone_sections(const BaseTopology& base, ZoneTopology& zone);
    [[nodiscard]] bool read_section_topology(const BaseTopology& base, const ZoneTopology& zone, SectionTopology& section);

    [[nodiscard]] bool build_structured_section(ZoneTopology& zone);

    bool fatten_section_elem_normal(const SectionTopology& section);
    bool fatten_section_elem_mixed(const SectionTopology& section);
    void fatten_section_elem_poly(bool separate_surface);

    std::vector<ReaderAPI::Node> m_node_coordinates;
    std::vector<ReaderAPI::Elem> m_elements;
    std::unordered_map<std::string, std::vector<ReaderAPI::Integer>> m_components;

    std::vector<SectionTopology> m_ngon_nface;

    cgsize_t m_node_offset = 0;
    cgsize_t m_element_offset = 0;
};
