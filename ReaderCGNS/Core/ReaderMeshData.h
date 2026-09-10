#pragma once
#include "FileManager.h"
#include "CgnsTopology.hpp"

#include <span>

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
    [[nodiscard]] bool build_connectivity_components();

private:
    [[nodiscard]] cgsize_t initialize_section_mixed(const SectionTopology& section,
                                                    std::vector<ReaderAPI::Elem>& elements,
                                                    const cgsize_t& element_offset,
                                                    const cgsize_t& node_offset) const;
    [[nodiscard]] cgsize_t initialize_section_normal(const SectionTopology& section,
                                                     std::vector<ReaderAPI::Elem>& elements,
                                                     const cgsize_t& element_offset,
                                                     const cgsize_t& node_offset) const;
    cgsize_t initialize_section_ngon_nface(std::string_view base_name,
                                           const ZoneTopology& zone_topology,
                                           std::vector<ReaderAPI::Elem>& elements,
                                           cgsize_t& element_offset,
                                           cgsize_t node_offset,
                                           bool flag);

    void update_components(std::string& component_name, std::size_t element_count, cgsize_t element_start);

    [[nodiscard]] bool read_base_topology(int index_base, std::span<const int> zone_indices, BaseTopology& base) const;
    [[nodiscard]] bool read_zone_topology(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_zone_coordinates(int index_base, int index_zone, ZoneTopology& zone) const;
    bool read_unstructured_zone_sections(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_section_topology(int index_base, int index_zone, int index_section, SectionTopology& section) const;

    [[nodiscard]] bool build_structured_section(ZoneTopology& zone) const;

    std::vector<BaseTopology> m_grid_topology;
    std::vector<ReaderAPI::Elem> m_elements;
    std::unordered_map<std::string, std::vector<ReaderAPI::Integer>> m_components;
};
