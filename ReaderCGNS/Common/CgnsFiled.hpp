#pragma once
#include "CgnsTypes.hpp"

#include <string>
#include <vector>

struct FieldFunction
{
    std::string name;
    CG_GridLocation_t location;
    std::vector<float> values;
};
