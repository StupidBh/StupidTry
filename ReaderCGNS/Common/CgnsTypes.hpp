#pragma once
#include "Utils/Utils.hpp"

#include "cgnslib.h"

#include <string>
#include <variant>
#include <vector>

inline static constexpr auto CGNS_NAME_MAX_LEN = 33;

struct FixedElementConnectivity
{
    int nodes_per_element = 0;
    std::vector<cgsize_t> values;
};

struct VariableElementConnectivity
{
    std::vector<cgsize_t> values;
    std::vector<cgsize_t> offsets;
};

using ElementConnectivity = std::variant<FixedElementConnectivity, VariableElementConnectivity>;

struct SectionTopology
{
    int index = 0;
    std::string name;
    CG_ElementType_t element_type = CG_ElementType_t::CG_ElementTypeNull;
    cgsize_t range_start = 0;
    cgsize_t range_end = 0;
    int boundary_element_count = 0;
    bool has_parent_data = false;
    ElementConnectivity connectivity;
    std::vector<cgsize_t> parent_data;
};

struct StructuredZoneTopology
{
    std::vector<cgsize_t> vertex_size;
    std::vector<cgsize_t> cell_size;
    std::vector<cgsize_t> boundary_vertex_size;
};

struct UnstructuredZoneTopology
{
    cgsize_t vertex_count = 0;
    cgsize_t cell_count = 0;
    cgsize_t boundary_vertex_count = 0;
    std::vector<SectionTopology> sections;
};

using ZoneData = std::variant<StructuredZoneTopology, UnstructuredZoneTopology>;

struct ZoneTopology
{
    int index = 0;
    std::string name;
    CG_ZoneType_t zone_type = CG_ZoneType_t::CG_ZoneTypeNull;
    int index_dimension = 0;
    ZoneData data;
};

struct BaseTopology
{
    int index = 0;
    std::string name;
    int cell_dimension = 0;
    int physical_dimension = 0;
    std::vector<ZoneTopology> zones;
};
