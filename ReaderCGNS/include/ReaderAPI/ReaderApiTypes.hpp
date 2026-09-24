#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ReaderAPI {
    using Integer = std::int32_t;
    using Real = float;

    struct Node
    {
        Integer id;
        Real x;
        Real y;
        Real z;
    };

    struct Field
    {
        std::string var;
        std::vector<std::string> sub_vars;
    };
} // namespace ReaderAPI
