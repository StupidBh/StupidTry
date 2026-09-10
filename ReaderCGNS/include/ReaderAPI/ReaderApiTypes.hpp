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

    struct Elem
    {
        Integer id;
        Integer type;
        Integer npts;
        std::vector<Integer> nodes;
    };

    struct Field
    {
        std::string name;
        Integer type; // 0-Node | 1-CellCenter | 2-FaceCenter | 3-PointSet

        std::vector<Integer> ids;
        std::vector<Real> values;

        bool isEmpty() const noexcept { return this->name.empty() || this->ids.empty() || this->values.empty() || this->ids.size() != this->values.size(); }
    };
} // namespace ReaderAPI
