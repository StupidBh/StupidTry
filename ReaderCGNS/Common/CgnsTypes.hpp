#pragma once
#include <string>

#include "cgnslib.h"
#include "Utils/Utils.hpp"

inline static constexpr auto CGNS_NAME_MAX_LEN = 33;

template<class T>
struct Topology
{
    T type { };
    int index = 0;
    std::string name;
};

struct SectionTopology : Topology<CG_ElementType_t>
{
    SectionTopology() :
        Topology(CG_ElementType_t::CG_ElementTypeNull)
    {
    }

    cgsize_t range_start = 0;
    cgsize_t range_end = 0;
    std::vector<cgsize_t> elements;
    std::vector<cgsize_t> connect_offset;

    bool has_parent_data = false;
};

struct ZoneTopology : Topology<CG_ZoneType_t>
{
    ZoneTopology() :
        Topology(CG_ZoneType_t::CG_ZoneTypeNull)
    {
        this->zone_size.fill(0);
    }

    int dim = 0;
    std::array<cgsize_t, 9> zone_size = { };
    std::array<std::vector<float>, 3> coordinates_xyz;

    std::vector<SectionTopology> sections;
};

struct BaseTopology : Topology<CG_SimulationType_t>
{
    BaseTopology() :
        Topology(CG_SimulationType_t::CG_SimulationTypeNull)
    {
    }

    int cell_dim = 0;
    int phy_dim = 0;

    std::vector<ZoneTopology> zones;
};
