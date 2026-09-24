#pragma once
#include "CgnsTypes.hpp"

#include <unordered_map>

struct FieldIndex
{
    int base;
    int zone;
    int solution;
    int field;
};

struct ZoneOffset
{
    cgsize_t node_offset;
    cgsize_t cell_offset;
};

template<class T>
using BaseZoneMap = std::unordered_map<int, std::unordered_map<int, T>>;

using BaseZoneOffset = BaseZoneMap<ZoneOffset>;
using SolutionLocation = BaseZoneMap<std::unordered_map<int, CG_GridLocation_t>>;
