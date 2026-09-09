#pragma once
#include <cstdint>
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

    struct Elem
    {
        Integer id;
        Integer type;
        Integer npts;
        std::vector<Integer> nodes;
    };
} // namespace ReaderAPI
