#pragma once
#include "FileManager.h"
#include "CgnsTopology.hpp"

#include <optional>
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
    [[nodiscard]] std::optional<ReaderAPI::Integer> initialize_section_mixed(const SectionTopology& section,
                                                                             std::vector<ReaderAPI::Elem>& elements,
                                                                             const ReaderAPI::Integer& element_offset,
                                                                             const ReaderAPI::Integer& node_offset) const;
    [[nodiscard]] std::optional<ReaderAPI::Integer> initialize_section_normal(const SectionTopology& section,
                                                                              std::vector<ReaderAPI::Elem>& elements,
                                                                              const ReaderAPI::Integer& element_offset,
                                                                              const ReaderAPI::Integer& node_offset) const;

private:
    [[nodiscard]] bool read_base_topology(int index_base, std::span<const int> zone_indices, BaseTopology& base) const;
    [[nodiscard]] bool read_zone_topology(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_zone_coordinates(int index_base, int index_zone, ZoneTopology& zone) const;
    bool read_unstructured_zone_sections(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_section_topology(int index_base, int index_zone, int index_section, SectionTopology& section) const;

    [[nodiscard]] bool build_structured_section(ZoneTopology& zone) const;

    std::vector<BaseTopology> m_grid_topology;
};
