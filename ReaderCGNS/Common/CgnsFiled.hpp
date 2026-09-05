#pragma once
#include "CgnsTypes.hpp"

#include <string>
#include <vector>

#include "cgnslib.h"

struct FieldFunction
{
    std::string name;
    CG_GridLocation_t location;
    std::vector<float> values;
};
