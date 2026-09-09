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

    /// @brief 表示一个标量场及其关联的网格实体。
    ///
    /// `ids` 与 `values` 按索引一一对应：`values[i]` 是 `ids[i]` 所标识
    /// 网格实体上的场值。实体编号采用 ReaderAPI 合并各 Zone 后的全局 0-based 编号。
    struct Field
    {
        /// @brief 场名称。
        std::string name;

        /// @brief 场值所关联的网格实体类型。
        ///
        /// 类型编码为：`0` 表示 Vertex，`1` 表示 CellCenter；`2`（FaceCenter）和
        /// `3`（PointSet）为预留值。当前 ReaderCGNS 实现仅返回 `0` 或 `1`，`-1` 表示未初始化。
        Integer type = -1;

        /// @brief 与场值对应的网格实体 ID。
        ///
        /// `ids[i]` 标识 `values[i]` 所属的节点或单元，容器长度必须与 `values` 相同。
        std::vector<Integer> ids;

        /// @brief 各网格实体上的标量场值。
        ///
        /// `values[i]` 是 `ids[i]` 所标识实体上的值，容器长度必须与 `ids` 相同。
        std::vector<Real> values;

        /// @brief 检查场数据是否为空或不完整。
        /// @retval true 名称、实体 ID 或场值为空，或者 `ids` 与 `values` 的长度不同。
        /// @retval false 名称和数据均非空，且每个实体 ID 都有对应的场值。
        bool isEmpty() const noexcept { return this->name.empty() || this->ids.empty() || this->values.empty() || this->ids.size() != this->values.size(); }
    };
} // namespace ReaderAPI
