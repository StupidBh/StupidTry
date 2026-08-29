# CGNS 4.5.1 C API 开发参考

> 本文以仓库内 `ReaderCGNS/3rdparty/cgns/include/cgnslib.h` 为接口边界，覆盖其中全部
> `cg_*` C 函数。节点语义、数据树和 element 布局见 [CGNS.md](CGNS.md)。精确参数类型
> 始终以实际编译使用的头文件为准；官方 MLL 文档用于解释参数约束和行为。

## 1. 如何使用本文

先按“节点/任务 -> API 组”定位，再查看本组的调用规则；需要精确签名时直接查 `cgnslib.h`。函数目录有意保留每个完整函数名，使代码代理可以检索并核对 API 覆盖。

CGNS MLL 分为两类接口：

- **显式定位接口**：参数含 `fn, B, Z, ...`，调用本身确定节点，如 `cg_zone_read`、`cg_field_read`。
- **当前位置接口**：先用 `cg_goto/cg_gorel/cg_gopath/cg_golist` 导航，再调用不带 `fn/B/Z` 的接口，如 `cg_array_read`、`cg_units_read`、`cg_descriptor_read`。当前位置属于库状态，不应假定跨文件或并发调用后仍不变。

## 2. 通用约定

### 2.1 返回值、模式和索引

除 `void` 或返回字符串的少数函数外，函数返回 `CG_OK`、`CG_ERROR` 或 `CG_NODE_NOT_FOUND`。每次调用都应检查；错误文本由 `cg_get_error()` 获取。

| 项目 | 约定 |
|---|---|
| 文件模式 | `CG_MODE_READ`、`CG_MODE_WRITE`、`CG_MODE_MODIFY` |
| 文件类型 | `CG_FILE_NONE`、`CG_FILE_ADF`、`CG_FILE_HDF5`、`CG_FILE_ADF2` |
| 对象索引 | `B/Z/S/F/C/BC/...` 均从 1 开始，含义由当前函数决定 |
| 网格索引 | 顶点号、元素号和 PointSet 值从 1 开始 |
| 范围 | `rmin/rmax` 是包含两端的 CGNS 范围，不是 C 的半开区间 |
| 大小类型 | 网格尺寸和索引使用 `cgsize_t`；不要假定其等于 `int` 或固定 64 位 |
| 名称缓冲 | 至少 `CG_MAX_NAME_LENGTH + 1`；标准 CGNS 名称最大 32 字符 |
| 枚举 | 声明和值使用 `CGNS_ENUMT(T)` / `CGNS_ENUMV(V)`，兼容 scoped-enum 构建 |

`cg_version(fn, &v)` 返回文件标准版本；编译时库版本是 `CGNS_VERSION`/`CGNS_DOTVERS`。两者不可互换。

### 2.2 数据类型和数组布局

`DataType_t` 包括 `Integer(I4)`、`LongInteger(I8)`、`RealSingle(R4)`、`RealDouble(R8)`、`Character(C1)`、`ComplexSingle(X4)`、`ComplexDouble(X8)`。许多 read API 允许指定目标内存类型并执行数值转换。

- 普通 `*_read`：从文件中的 CGNS 范围读入连续目标缓冲区。
- `*_partial_write/read`：只操作文件数组的一个连续 CGNS 范围。
- `*_general_read/write`：分别描述文件空间 `s_*` 和内存空间 `m_*`，支持类型转换、维度不同和内存子区域。
- CGNS 第一维变化最快。`m_dims/m_rmin/m_rmax` 描述内存逻辑布局，不是字节 stride。
- `MIXED/NGON_n/NFACE_n` 的 offset 是 connectivity 缓冲区中的 0-based 偏移；网格 ID 仍为 1-based。

### 2.3 所有权和生命周期

- 调用者为绝大多数输出数组和名称缓冲分配内存。
- `cg_descriptor_read`、`cg_convergence_read`、`cg_geo_read`、`cg_link_read`、`cg_state_read` 可能返回库分配内存；完成后用 `cg_free()`，尤其在 Windows DLL 边界不要直接假定 CRT allocator 相同。
- `cg_get_error()`、所有 `cg_*Name()` 返回库拥有的只读字符串，不释放、不修改；后续调用可能改变错误文本。
- `cg_1to1_read_global` 的二维输出缓冲由调用者按 `cg_n1to1_global` 返回的数量预先分配；不要把它与上述库分配字符串接口混淆。
- `*_id` 返回的 `double` 是 CGIO 节点 ID，不是可序列化业务标识，关闭文件后失效。

### 2.4 导航

```c
cg_goto(fn, B,
        "Zone_t", Z,
        "FlowSolution_t", S,
        "end");
```

标签配正索引；名称配索引 `0`。`cg_gorel` 从当前位置开始，`"..", 0` 表示父节点；`cg_gopath` 按名称路径；`cg_where` 返回当前位置的 label/index 路径。所有依赖当前位置的读写前都应在相邻代码中明确导航。

## 3. API 全目录

### 3.1 文件、库配置、CGIO 和错误处理

| 功能 | API |
|---|---|
| 探测、打开、版本、精度、保存和关闭 | `cg_is_cgns`, `cg_open`, `cg_version`, `cg_precision`, `cg_save_as`, `cg_close` |
| 文件类型 | `cg_set_file_type`, `cg_get_file_type` |
| CGIO 互操作 | `cg_root_id`, `cg_get_cgio` |
| 库配置 | `cg_configure`, `cg_error_handler`, `cg_set_compress`, `cg_get_compress`, `cg_set_path`, `cg_add_path` |
| 错误 | `cg_get_error`, `cg_error_print`, `cg_error_exit` |
| 库内存 | `cg_free` |

`cg_is_cgns` 成功识别时同时返回物理文件类型。`cg_save_as` 可转换后端/保存副本；写入中的同一文件不能由多个进程同时修改。`cg_set_path/cg_add_path` 只配置外部链接搜索路径。`cg_error_exit` 会终止进程，库代码通常应返回错误而不是调用它。

### 3.2 枚举到名称

| 枚举族 | API |
|---|---|
| 通用表查询 | `cg_get_name` |
| 基本单位 | `cg_MassUnitsName`, `cg_LengthUnitsName`, `cg_TimeUnitsName`, `cg_TemperatureUnitsName`, `cg_AngleUnitsName` |
| 扩展单位 | `cg_ElectricCurrentUnitsName`, `cg_SubstanceAmountUnitsName`, `cg_LuminousIntensityUnitsName` |
| 数据/位置/点集 | `cg_DataClassName`, `cg_GridLocationName`, `cg_PointSetTypeName`, `cg_DataTypeName` |
| BC/连通 | `cg_BCDataTypeName`, `cg_BCTypeName`, `cg_GridConnectivityTypeName` |
| 方程和模型 | `cg_GoverningEquationsTypeName`, `cg_ModelTypeName`, `cg_ParticleGoverningEquationsTypeName`, `cg_ParticleModelTypeName` |
| 网格和运动 | `cg_ElementTypeName`, `cg_ZoneTypeName`, `cg_RigidGridMotionTypeName`, `cg_ArbitraryGridMotionTypeName`, `cg_SimulationTypeName` |
| 属性 | `cg_WallFunctionTypeName`, `cg_AreaTypeName`, `cg_AverageInterfaceTypeName` |

不要持久化枚举整数或自行索引 `*Name[]` 全局表；使用类型化名称函数，并处理 Null/UserDefined/未知值。

### 3.3 Base 与 Zone

| 节点 | API |
|---|---|
| `CGNSBase_t` | `cg_nbases`, `cg_base_read`, `cg_base_id`, `cg_base_write`, `cg_cell_dim` |
| `Zone_t` | `cg_nzones`, `cg_zone_read`, `cg_zone_type`, `cg_zone_id`, `cg_zone_write`, `cg_index_dim` |

`cg_base_read` 给出 cell/physical dimension。调用 `cg_zone_read` 前后必须用 `cg_zone_type`/`cg_index_dim` 决定 `size` 缓冲的解释：Structured 为 `IndexDimension x 3`，Unstructured 为 3 个值。

### 3.4 Family 与外部几何

| 节点 | API |
|---|---|
| Base 级 `Family_t` | `cg_nfamilies`, `cg_family_read`, `cg_family_write`, `cg_nfamily_names`, `cg_family_name_read`, `cg_family_name_write` |
| 当前节点下嵌套 family | `cg_node_nfamilies`, `cg_node_family_read`, `cg_node_family_write`, `cg_node_nfamily_names`, `cg_node_family_name_read`, `cg_node_family_name_write` |
| `FamilyName_t` / additional family | `cg_famname_read`, `cg_famname_write`, `cg_nmultifam`, `cg_multifam_read`, `cg_multifam_write` |
| Base family BC | `cg_fambc_read`, `cg_fambc_write` |
| 当前 family 的 BC | `cg_node_fambc_read`, `cg_node_fambc_write` |
| `GeometryReference_t` | `cg_geo_read`, `cg_geo_write`, `cg_node_geo_read`, `cg_node_geo_write` |
| `GeometryEntity_t` | `cg_part_read`, `cg_part_write`, `cg_node_part_read`, `cg_node_part_write` |

`cg_node_*` 和 `cg_famname/cg_multifam_*` 依赖当前位置；先导航到允许这些子节点的父节点。Family 名可为跨 Base 或嵌套绝对路径，不要截断到最后一段。

### 3.5 坐标与网格包围盒

| 节点/操作 | API |
|---|---|
| `GridCoordinates_t` 容器 | `cg_ngrids`, `cg_grid_read`, `cg_grid_write` |
| 包围盒 | `cg_grid_bounding_box_read`, `cg_grid_bounding_box_write` |
| 坐标数组元数据/ID | `cg_ncoords`, `cg_coord_info`, `cg_coord_id` |
| 坐标读取 | `cg_coord_read`, `cg_coord_general_read` |
| 坐标写入 | `cg_coord_write`, `cg_coord_partial_write`, `cg_coord_general_write` |

专用 `cg_coord_*` 默认访问当前 Zone 选定/默认的坐标节点。多套网格时用 `cg_grid_*` 枚举节点，并在需要访问非默认节点的通用元数据时导航到具体 `GridCoordinates_t`。

### 3.6 Element sections

| 操作 | API |
|---|---|
| Section 数量/元数据 | `cg_nsections`, `cg_section_read`, `cg_npe`, `cg_ElementDataSize`, `cg_ElementPartialSize` |
| 整段读取 | `cg_elements_read`, `cg_poly_elements_read` |
| 部分读取 | `cg_elements_partial_read`, `cg_poly_elements_partial_read` |
| 指定内存整数类型读取 | `cg_elements_general_read`, `cg_poly_elements_general_read`, `cg_parent_elements_general_read`, `cg_parent_elements_position_general_read` |
| 新建 section | `cg_section_write`, `cg_poly_section_write`, `cg_section_general_write`, `cg_section_partial_write`, `cg_section_initialize` |
| 写 connectivity | `cg_elements_partial_write`, `cg_elements_general_write`, `cg_poly_elements_partial_write`, `cg_poly_elements_general_write` |
| 写 parent data | `cg_parent_data_write`, `cg_parent_data_partial_write` |

先用 `cg_section_read` 获取类型、范围、边界数和 parent 标志，再用 `cg_ElementDataSize` 精确分配。固定拓扑用 `cg_elements_*`；`MIXED/NGON_n/NFACE_n` 用 `cg_poly_elements_*` 并分配 `ElementSize + 1` 个 offset。头文件将 general element read 标记为“use at your own risk”；只有确需不同于 `cgsize_t` 的内存整数类型时使用。

### 3.7 FlowSolution 与字段

| 节点/操作 | API |
|---|---|
| Solution 元数据 | `cg_nsols`, `cg_sol_info`, `cg_sol_id`, `cg_sol_size` |
| Solution 写入 | `cg_sol_write` |
| 稀疏 solution PointSet | `cg_sol_ptset_info`, `cg_sol_ptset_read`, `cg_sol_ptset_write` |
| 字段元数据/ID | `cg_nfields`, `cg_field_info`, `cg_field_id` |
| 字段读取 | `cg_field_read`, `cg_field_general_read` |
| 字段写入 | `cg_field_write`, `cg_field_partial_write`, `cg_field_general_write` |

`cg_sol_info` 的 `GridLocation` 决定字段维度。完整 solution、带 PointSet 的稀疏 solution、带 Rind 的 structured solution 长度不同，应使用 `cg_sol_size`，不能仅从 Zone cell count 猜测。

### 3.8 ZoneSubRegion 与 DiscreteData

| 节点 | API |
|---|---|
| `ZoneSubRegion_t` 查询 | `cg_nsubregs`, `cg_subreg_info`, `cg_subreg_ptset_read`, `cg_subreg_bcname_read`, `cg_subreg_gcname_read` |
| `ZoneSubRegion_t` 写入 | `cg_subreg_ptset_write`, `cg_subreg_bcname_write`, `cg_subreg_gcname_write` |
| `DiscreteData_t` 元数据 | `cg_ndiscrete`, `cg_discrete_read`, `cg_discrete_size` |
| `DiscreteData_t` 写入 | `cg_discrete_write` |
| Discrete PointSet | `cg_discrete_ptset_info`, `cg_discrete_ptset_read`, `cg_discrete_ptset_write` |

SubRegion 用 PointSet、BC 名引用、GridConnectivity 名引用三种方式之一定义。其附加数组和 DiscreteData 的实际数组均通过导航后的 `cg_array_*` 访问。

### 3.9 Zone connectivity 与 overset

| 节点/操作 | API |
|---|---|
| 多个 `ZoneGridConnectivity_t` 容器 | `cg_nzconns`, `cg_zconn_read`, `cg_zconn_write`, `cg_zconn_get`, `cg_zconn_set` |
| `OversetHoles_t` | `cg_nholes`, `cg_hole_info`, `cg_hole_read`, `cg_hole_id`, `cg_hole_write` |
| 一般 `GridConnectivity_t` | `cg_nconns`, `cg_conn_info`, `cg_conn_read`, `cg_conn_id`, `cg_conn_write`, `cg_conn_write_short`, `cg_conn_read_short` |
| Zone 内 1-to-1 | `cg_n1to1`, `cg_1to1_read`, `cg_1to1_id`, `cg_1to1_write` |
| Base 全局 1-to-1 | `cg_n1to1_global`, `cg_1to1_read_global` |

先 `cg_conn_info` 获取本区位置/点集、donor Zone/type/点集、donor 数据类型和数量，再分配两个编号空间的缓冲。short 变体省略 donor 数据，只适合明确不需要完整 donor 映射的场景。多个 connectivity 容器存在时，`cg_zconn_set` 选择后续专用 API 操作的当前容器。

### 3.10 边界条件与边界数据

| 节点/操作 | API |
|---|---|
| `BC_t` 元数据和数据 | `cg_nbocos`, `cg_boco_info`, `cg_boco_read`, `cg_boco_id`, `cg_boco_write` |
| 法向和位置 | `cg_boco_normal_write`, `cg_boco_gridlocation_read`, `cg_boco_gridlocation_write` |
| BC 下 `BCDataSet_t` | `cg_dataset_read`, `cg_dataset_write` |
| Family BC dataset（当前位置） | `cg_bcdataset_info`, `cg_bcdataset_read`, `cg_bcdataset_write` |
| `BCData_t` | `cg_bcdata_write` |

`cg_boco_info` 返回 PointSet 类型/数量、法向索引、法向数据类型和 dataset 数量；据此分配 `cg_boco_read` 的点与法向缓冲。非结构三维壁面通常使用 `FaceCenter + PointList`，其中值是面 element ID，而不是顶点 ID。dataset 中的实际物理量通过导航到 `BCData_t` 后使用 `cg_array_*`。

### 3.11 网格运动、迭代和全局几何状态

| 节点 | API |
|---|---|
| `RigidGridMotion_t` | `cg_n_rigid_motions`, `cg_rigid_motion_read`, `cg_rigid_motion_write` |
| `ArbitraryGridMotion_t` | `cg_n_arbitrary_motions`, `cg_arbitrary_motion_read`, `cg_arbitrary_motion_write` |
| `SimulationType_t` | `cg_simulation_type_read`, `cg_simulation_type_write` |
| `BaseIterativeData_t` | `cg_biter_read`, `cg_biter_write` |
| `ZoneIterativeData_t` | `cg_ziter_read`, `cg_ziter_write` |
| `ParticleIterativeData_t` | `cg_piter_read`, `cg_piter_write` |
| `Gravity_t` | `cg_gravity_read`, `cg_gravity_write` |
| `Axisymmetry_t` | `cg_axisym_read`, `cg_axisym_write` |
| `RotatingCoordinates_t` | `cg_rotating_read`, `cg_rotating_write` |

`*_iter_*` 只创建/读取容器和步数；`TimeValues`、`FlowSolutionPointers` 等数组仍通过当前位置 `cg_array_*` 访问。`cg_rotating_*` 也依赖当前位置，使局部旋转定义能覆盖 Base 默认值。

### 3.12 BC 与 connectivity 属性

| 属性 | API |
|---|---|
| 壁面函数 | `cg_bc_wallfunction_read`, `cg_bc_wallfunction_write` |
| 面积 | `cg_bc_area_read`, `cg_bc_area_write` |
| 一般 connectivity 周期属性 | `cg_conn_periodic_read`, `cg_conn_periodic_write` |
| 1-to-1 周期属性 | `cg_1to1_periodic_read`, `cg_1to1_periodic_write` |
| 一般 connectivity 平均属性 | `cg_conn_average_read`, `cg_conn_average_write` |
| 1-to-1 平均属性 | `cg_1to1_average_read`, `cg_1to1_average_write` |

周期数据包含旋转中心、旋转角和 translation，数组长度由 `PhysicalDimension` 决定；单位和角度制应通过元数据明确。

### 3.13 粒子 Zone、坐标与解

| 节点/操作 | API |
|---|---|
| `ParticleZone_t` | `cg_nparticle_zones`, `cg_particle_id`, `cg_particle_read`, `cg_particle_write` |
| `ParticleCoordinates_t` 容器 | `cg_particle_ncoord_nodes`, `cg_particle_coord_node_read`, `cg_particle_coord_node_write` |
| 粒子包围盒 | `cg_particle_bounding_box_read`, `cg_particle_bounding_box_write` |
| 粒子坐标元数据/ID | `cg_particle_ncoords`, `cg_particle_coord_info`, `cg_particle_coord_id` |
| 粒子坐标读取 | `cg_particle_coord_read`, `cg_particle_coord_general_read` |
| 粒子坐标写入 | `cg_particle_coord_write`, `cg_particle_coord_partial_write`, `cg_particle_coord_general_write` |
| `ParticleSolution_t` | `cg_particle_nsols`, `cg_particle_sol_info`, `cg_particle_sol_id`, `cg_particle_sol_write`, `cg_particle_sol_size` |
| 粒子 solution PointSet | `cg_particle_sol_ptset_info`, `cg_particle_sol_ptset_read`, `cg_particle_sol_ptset_write` |
| 粒子字段元数据/ID | `cg_particle_nfields`, `cg_particle_field_info`, `cg_particle_field_id` |
| 粒子字段读取 | `cg_particle_field_read`, `cg_particle_field_general_read` |
| 粒子字段写入 | `cg_particle_field_write`, `cg_particle_field_partial_write`, `cg_particle_field_general_write` |

粒子 API 的 `P` 是 ParticleZone 索引，不是 PointSet 或 particle ID。粒子数组通常是一维 particle 序列；稀疏 solution 的精确长度由 `cg_particle_sol_size`/PointSet 元数据确定。

### 3.14 通用导航、删除和链接

| 操作 | API |
|---|---|
| 绝对/相对导航 | `cg_goto`, `cg_goto_f08`, `cg_gorel`, `cg_gorel_f08`, `cg_gopath`, `cg_golist`, `cg_where` |
| 链接 | `cg_is_link`, `cg_link_read`, `cg_link_write` |
| 删除 | `cg_delete_node` |

`cg_goto_f08/cg_gorel_f08` 是 Fortran 2008 互操作入口，普通 C 代码使用非 `_f08` 版本。`cg_delete_node` 删除当前位置的指定直接子节点，仅在 modify/write 场景使用；节点名必须精确匹配。链接目标可以跨文件，部署时必须同步管理被链接文件和搜索路径。

### 3.15 收敛、参考状态与方程模型

| 节点 | API |
|---|---|
| `ConvergenceHistory_t` | `cg_convergence_read`, `cg_convergence_write` |
| `ReferenceState_t` | `cg_state_read`, `cg_state_write` |
| `FlowEquationSet_t` | `cg_equationset_read`, `cg_equationset_chemistry_read`, `cg_equationset_elecmagn_read`, `cg_equationset_write` |
| `ParticleEquationSet_t` | `cg_particle_equationset_read`, `cg_particle_equationset_write` |
| `GoverningEquations_t` | `cg_governing_read`, `cg_governing_write` |
| 扩散标志 | `cg_diffusion_read`, `cg_diffusion_write` |
| 连续相模型节点 | `cg_model_read`, `cg_model_write` |
| `ParticleGoverningEquations_t` | `cg_particle_governing_read`, `cg_particle_governing_write` |
| 粒子模型节点 | `cg_particle_model_read`, `cg_particle_model_write` |

本组均依赖当前位置。`cg_model_*` 的 `ModelLabel` 决定访问 `GasModel_t`、`ViscosityModel_t`、`TurbulenceModel_t` 等哪个节点；不是每个 `ModelType_t` 都对每个 label 合法。模型参数使用该模型节点下的 `DataArray_t`。

### 3.16 通用 DataArray 和扩展容器

| 节点 | API |
|---|---|
| `DataArray_t` 查询 | `cg_narrays`, `cg_array_info` |
| 数组读取 | `cg_array_read`, `cg_array_read_as`, `cg_array_general_read` |
| 数组写入 | `cg_array_write`, `cg_array_general_write` |
| `UserDefinedData_t` | `cg_nuser_data`, `cg_user_data_read`, `cg_user_data_write` |
| `IntegralData_t` | `cg_nintegrals`, `cg_integral_read`, `cg_integral_write` |

必须先导航到数组父节点。`cg_array_info` 最多写 `CG_MAX_DIMENSIONS` 个维度值；按返回类型和维度乘积做溢出检查后再分配。`cg_array_read` 按文件类型读取，`cg_array_read_as` 执行目标类型转换。

### 3.17 通用元数据节点

| 节点 | API |
|---|---|
| `Rind_t` | `cg_rind_read`, `cg_rind_write` |
| `Descriptor_t` | `cg_ndescriptors`, `cg_descriptor_read`, `cg_descriptor_write` |
| `DimensionalUnits_t` | `cg_nunits`, `cg_units_read`, `cg_units_write`, `cg_unitsfull_read`, `cg_unitsfull_write` |
| `DimensionalExponents_t` | `cg_exponents_info`, `cg_nexponents`, `cg_exponents_read`, `cg_exponents_write`, `cg_expfull_read`, `cg_expfull_write` |
| `DataConversion_t` | `cg_conversion_info`, `cg_conversion_read`, `cg_conversion_write` |
| `DataClass_t` | `cg_dataclass_read`, `cg_dataclass_write` |
| `GridLocation_t` | `cg_gridlocation_read`, `cg_gridlocation_write` |
| `Ordinal_t` | `cg_ordinal_read`, `cg_ordinal_write` |
| 通用 PointSet | `cg_ptset_info`, `cg_ptset_read`, `cg_ptset_write` |

这些 API 都读取/写入当前位置允许的子节点。`cg_units_*` 是 5 基本单位旧形式，`cg_unitsfull_*` 是 8 单位完整形式；用 `cg_nunits` 判断实际数量。指数同理，用 `cg_nexponents` 区分 5 与 8 分量。`Ordinal` 无唯一性保证。

## 4. 典型安全读取顺序

```text
cg_is_cgns -> cg_open
  -> cg_nbases -> cg_base_read
    -> cg_nzones -> cg_zone_type + cg_zone_read + cg_index_dim
      -> cg_ngrids/cg_ncoords -> cg_coord_info -> cg_coord_read
      -> cg_nsections -> cg_section_read -> cg_ElementDataSize
         -> cg_elements_read 或 cg_poly_elements_read
      -> cg_nsols -> cg_sol_info + cg_sol_size
         -> cg_nfields -> cg_field_info -> cg_field_read
      -> cg_nbocos -> cg_boco_info -> cg_boco_read
      -> cg_nzconns/cg_nconns/cg_n1to1 -> 对应 info/read
  -> cg_close
```

原则是“先 info/size，再检查乘法溢出并分配，最后 read”。遇到未知枚举、非法维度、重叠 element range 或数组长度不匹配时应明确报错，不能猜测并继续。

## 5. General API 参数模型

以 `cg_field_general_read` 为例：

```text
s_rmin/s_rmax              文件数组中要读取的闭区间
m_type                     目标内存数据类型
m_numdim, m_dimvals        完整目标内存数组的秩和尺寸
m_rmin/m_rmax              写入目标内存的闭区间
field_ptr                  目标内存首地址
```

源区间与目标区间的元素总数必须相同。general write 还会同时给出文件存储类型 `s_type` 和内存类型 `m_type`。当目标就是连续一维向量时，普通 read/partial write 更清晰；general API 适合切片、类型转换和写入大型预分配数组的子区域。

## 6. 错误处理骨架

```cpp
#include <stdexcept>

class CgnsFile {
public:
    explicit CgnsFile(const char* path)
    {
        if (cg_open(path, CG_MODE_READ, &m_fn) != CG_OK) {
            throw std::runtime_error(cg_get_error());
        }
    }

    ~CgnsFile() { if (m_fn > 0) cg_close(m_fn); }

    CgnsFile(const CgnsFile&) = delete;
    CgnsFile& operator=(const CgnsFile&) = delete;

    int get() const noexcept { return m_fn; }

private:
    int m_fn = 0;
};
```

析构函数不能抛异常；若业务必须报告 close 失败，应提供显式 `close()` 并让析构只做兜底。复制 `cg_get_error()` 文本后再进行其他 CGNS 调用，避免错误文本被覆盖。

## 7. 官方和仓库参考

- [CGNS MLL C API 总览](https://cgns.org/standard/MLL/api/c_api.html)
- [CGNS MLL General Remarks 与枚举](https://cgns.org/standard/MLL/api/general_remarks.html)
- [CGNS SIDS](https://cgns.org/standard/SIDS/CGNS_SIDS.html)
- [CGNS File Mapping Manual](https://cgns.org/standard/CGNS_FMM.html)
- 仓库权威签名：`ReaderCGNS/3rdparty/cgns/include/cgnslib.h`

本文只覆盖串行 MLL `cgnslib.h`。并行 `cgp_*` API 属于 `pcgnslib.h`，CGIO 低层 `cgio_*` API 属于 `cgns_io.h`，二者不在当前工程公开使用的 CGNS C API 清单内。
