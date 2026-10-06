#pragma once
#include "CgnsTypes.hpp"

#include <array>
#include <vector>
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

    static constexpr std::array<cgsize_t, 3> r_min = { 1, 1, 1 };
    std::vector<cgsize_t> r_max;
};

template<class T>
using BaseZoneMap = std::unordered_map<int, std::unordered_map<int, T>>;

using BaseZoneOffset = BaseZoneMap<ZoneOffset>;
using SolutionLocation = BaseZoneMap<std::unordered_map<int, CG_GridLocation_t>>;
