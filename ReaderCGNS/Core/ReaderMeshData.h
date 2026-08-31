#pragma once
#include "FileManager.h"

#include <cstddef>
#include <span>
#include <vector>

class ReaderMeshData : virtual public FileManager {
public:
    ReaderMeshData() = default;
    ~ReaderMeshData() override = default;

private:
    bool initialize_file_data() override;
    void clear_file_data() noexcept override;

    [[nodiscard]] bool initialize_grid_topology();
    [[nodiscard]] bool read_base_topology(int base_index, std::span<const int> zone_indices, BaseTopology& base) const;
    [[nodiscard]] bool read_zone_topology(int base_index, int base_cell_dimension, int zone_index, ZoneTopology& zone) const;
    [[nodiscard]] bool read_structured_zone_topology(int index_dimension, std::span<const cgsize_t> zone_size, StructuredZoneTopology& topology) const;
    [[nodiscard]] bool
        read_unstructured_zone_topology(int base_index, int zone_index, std::span<const cgsize_t> zone_size, UnstructuredZoneTopology& topology) const;
    [[nodiscard]] bool read_section_topology(int base_index, int zone_index, int section_index, SectionTopology& section) const;
    [[nodiscard]] bool read_fixed_connectivity(int base_index,
                                               int zone_index,
                                               int section_index,
                                               std::size_t element_count,
                                               cgsize_t element_data_size,
                                               SectionTopology& section) const;
    [[nodiscard]] bool read_variable_connectivity(int base_index,
                                                  int zone_index,
                                                  int section_index,
                                                  std::size_t element_count,
                                                  cgsize_t element_data_size,
                                                  SectionTopology& section) const;

    std::vector<BaseTopology> m_grid_topology;
};
