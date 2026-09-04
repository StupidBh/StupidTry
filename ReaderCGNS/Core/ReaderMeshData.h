#pragma once
#include "FileManager.h"
#include "CgnsTypes.hpp"

#include <span>

class ReaderMeshData : virtual public FileManager {
public:
    ReaderMeshData() = default;
    ~ReaderMeshData() override = default;

    bool GetAllElementSetName(std::vector<std::string>& element_set_names) final;
    bool GetAllNodeCoordinates(std::vector<ReaderAPI::Node>& node_coordinates) final;

protected:
    void clear_grid_topology() noexcept;

    [[nodiscard]] bool initialize_grid_topology();

private:
    [[nodiscard]] bool read_base_topology(int index_base, std::span<const int> zone_indices, BaseTopology& base) const;
    [[nodiscard]] bool read_zone_topology(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_zone_coordinates(int index_base, int index_zone, ZoneTopology& zone) const;
    void read_unstructured_zone_sections(int index_base, int index_zone, ZoneTopology& zone) const;
    [[nodiscard]] bool read_section_topology(int index_base, int index_zone, int index_section, SectionTopology& section) const;

    [[nodiscard]] bool build_structured_section(ZoneTopology& zone) const;

    std::vector<BaseTopology> m_grid_topology;
};
