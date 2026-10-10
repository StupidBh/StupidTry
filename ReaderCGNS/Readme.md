# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 动态加载模块。它基于官方 CGNS Mid-Level Library
遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出，并提供名称列表、节点坐标、单元连接和场值查询。

当前公开能力定位为“结构检查、诊断与数据查询”：接口可读取文件版本和 Base
级方程类型、列出组件与场函数名称，以扁平数组返回节点坐标和受支持单元的连接关系，并读取满足下述限制的节点/单元中心场值；
`ReaderAPI::ReaderApiBase::info()` 负责输出更完整的元数据和连接摘要。

## 能力范围

当前检查流程覆盖：

- CGNS 存储类型、文件版本与数据精度；
- Base、Zone、迭代信息与结构化/非结构化尺寸；
- 首次查询组件名称、节点坐标或单元连接时按需构建 Base/Zone/Section 内部网格拓扑，包括坐标、Structured
  connectivity、固定单元连接表以及 `MIXED/NGON_n/NFACE_n` 变长连接表；
- FlowSolution、DiscreteData 与 ZoneSubRegion；
- 按名称读取单个或批量 FlowSolution 字段值，支持 `Vertex` 和 `CellCenter`；
- GridCoordinates 与元素 Section/Connectivity；
- 一对一连接、一般网格连接与 Overset Holes；
- BoundaryCondition 及其 DataSet；
- 刚体和任意网格运动；
- ParticleZone、粒子坐标与粒子解描述。

输入文件使用 `CG_MODE_READ` 打开。库不会修改 CGNS 文件，也不会主动创建日志文件；诊断信息交给调用方注册的回调，查询数据通过返回值和输出参数交付。

### 内部网格拓扑

`FileManager` 在 `Open()` 期间通过 `initialize_base_zone_layout()` 确定本次需要分析的 Base/Zone 位置，跳过 Zone
数量读取失败或没有普通 Zone 的 Base；因此仅含 ParticleZone 的文件无法成功打开。首次调用 `GetAllComponentName()`、
`GetAllNodeCoordinates()` 或 `GetAllElement()` 时，`ReaderMeshData` 遍历这组索引，读取临时 Base/Zone/Section
数据，并缓存合并后的节点坐标、展开单元与组件映射。`ReaderFieldData` 使用独立的按需缓存保存 FlowSolution
字段布局，查询场值时直接读取文件，不依赖网格拓扑缓存。当前网格拓扑不缓存边界条件或 Zone 间连接数据。

Structured Zone 不要求存在 `Elements_t`；`ReaderMeshData` 根据 `VertexSize` 和 `CellSize` 合成一个 Section，并按维度展开为
`BAR_2`、`QUAD_4` 或 `HEXA_8` 的 1-based connectivity。Unstructured Zone 会遍历 Section：固定元素类型通过 `cg_npe()`
校验每个元素的节点数并读取连续 connectivity；`MIXED`、`NGON_n` 和 `NFACE_n` 通过 `cg_poly_elements_read()` 同时保存
connectivity 与 `ElementStartOffset`。Section 声明 parent data 时只记录存在标志，不缓存 `ParentElements` 或
`ParentElementsPosition` 的原始数据。

`read_section_topology()` 统一验证 Section 元素范围为正且起止有序，并检查数量和数据长度能否由 `size_t` 表示。对于变长
Section，先检查数量上限，再分配 connectivity 和 `ElementSize + 1` 个 offsets；读取成功后检查 offsets 首项为 0、末项等于
connectivity 长度且单调不减。只有通过这些检查的 Section 才会交给展开逻辑。`fatten_section_elem_poly()` 依赖该前置条件，不重复检查每条记录的
offsets 范围，保留依赖当前节点/单元偏移和面引用的检查。

网格初始化在遍历期间直接向成员缓存追加结果。Base 读取失败会返回 `false`，Zone 或 Section
读取失败通常记录日志并跳过；发生中途失败时，已经生成的内部缓存不会自动回滚。`Open()` 只负责文件和 Base/Zone
布局初始化。节点和单元查询以单元缓存是否为空决定是否初始化，组件查询以组件映射是否为空决定是否初始化；当前没有独立的“初始化已完成”标志。重复查询通常复用缓存，失败后重试和空结果的行为见下文[当前网格读取限制](#当前网格读取限制)。

## 公开接口

公开头文件位于仓库根目录的 [`ReaderAPI/`](../ReaderAPI/)，其中单元类型定义位于 `ReaderAPI/Types/ElementTypes.hpp`。将仓库根目录加入 include 路径后使用：

```cpp
#include "ReaderAPI/ReaderApiBase.h"
```

该头文件同时提供 `ReaderAPI::ReaderApiBase`、可选的 `ReaderAPI::FluidExtensionsBase`、reader 工厂函数指针类型，以及
`ReaderAPI::Logger::LogLevel` 和 `LogCallback` 日志协议类型。

| API                                                                    | 作用                                                                                          |
|------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| `ReaderAPI::ReaderApiBase::SetLogCallback(callback, context)`          | 为当前 reader 注册日志回调和可选上下文。                                                      |
| `ReaderAPI::ReaderApiBase::ClearLogCallback()`                         | 清除当前 reader 的日志回调。                                                                  |
| `ReaderAPI::ReaderApiBase::Open(path)`                                 | 以只读方式打开 CGNS 文件。                                                                    |
| `ReaderAPI::ReaderApiBase::Close()`                                    | 关闭当前文件。                                                                                |
| `ReaderAPI::ReaderApiBase::IsOpen()`                                   | 查询文件是否已打开。                                                                          |
| `ReaderAPI::ReaderApiBase::GetVersion()`                               | 返回当前文件记录的 CGNS 版本。                                                                |
| `ReaderAPI::ReaderApiBase::GetSolverType()`                            | 返回 Base/Zone 布局中第一个有效 Base 下 `FlowEquationSet_t/GoverningEquations_t` 的类型名称。 |
| `ReaderAPI::ReaderApiBase::GetAllComponentName(names)`                 | 用当前文件的组件名称替换输出数组。                                                            |
| `ReaderAPI::ReaderApiBase::GetComponent(name, ids)`                    | 用指定组件的扁平单元 ID 替换输出数组。                                                        |
| `ReaderAPI::ReaderApiBase::GetAllFieldFunctionName(names)`             | 返回按名称排序的物理场分组，替换调用方的 `std::vector<Field>`。                               |
| `ReaderAPI::ReaderApiBase::GetAllNodeCoordinates(nodes)`               | 用可读 Zone 的节点坐标替换输出数组。                                                          |
| `ReaderAPI::ReaderApiBase::GetAllElement(std::vector<Elem>& elements)` | 用缓存中的单元类型和连接数据替换 `std::vector<Elem>` 输出。                                   |
| `ReaderAPI::ReaderApiBase::GetAllElement(ElementTable& elements)`      | 用缓存中的单元类型和连接数据填充并替换 `ElementTable` 输出。                                  |
| `ReaderAPI::ReaderApiBase::GetFieldFunctionPosition(var)`             | 按组名查询组内所有子字段的统一位置；`0` 为节点，`1` 为单元中心，失败返回 `-1`。               |
| `ReaderAPI::ReaderApiBase::GetFieldFunctionIds(var, sub_var, ids)`    | 严格按组名与子字段名查询全局实体 ID，成功时追加到输出数组。                                    |
| `ReaderAPI::ReaderApiBase::GetFieldFunctionData(var, sub_var, data)`  | 严格按组名与子字段名读取场值，成功读取的值追加到输出数组。                                      |
| `ReaderAPI::ReaderApiBase::info()`                                     | 遍历当前已打开文件并输出结构检查信息。                                                        |
| `ReaderAPI::ReaderApiBase::GetFluidExtensions()`                     | 获取 reader 拥有的可选流体扩展接口；默认实现返回 `nullptr`。                                  |
| `ReaderAPI::FluidExtensionsBase::HasVelocityField()`                 | 检查当前文件的字段名称中是否包含所需速度分量。                                                |
| `ReaderAPI::FluidExtensionsBase::GetVelocityFieldPosition()`        | 查询三个速度分量的共同位置：`0` 为节点，`1` 为单元中心，失败返回 `-1`。                        |
| `ReaderAPI::FluidExtensionsBase::GetVelocityField(data, ids)`       | 读取按 X/Y/Z 排列的三个分量数组和它们共同的实体 ID。                                           |

所有数据查询都要求文件已成功打开。`GetAllNodeCoordinates()`、两个 `GetAllElement()` 重载、`GetAllComponentName()` 和
`GetComponent()` 成功时替换输出对象；按需初始化返回 `false` 时保留调用方输出。`GetAllFieldFunctionName()`
成功时替换输出分组数组，组名和各组子字段名均按名称排序。正常首次构建时，`Node::id` 从 0
开始按被接受的 Zone 累加生成。场值与字段 ID 查询的追加语义见下文。

组件名称和成员 ID 的当前语义如下：

| 来源                       | 组件名称                                       |
|----------------------------|------------------------------------------------|
| Structured Zone            | `Base.Zone`                                    |
| 固定类型或 `MIXED` Section | `Base.Zone.Section`                            |
| `NGON_n` Section           | `Base.Zone.Section`，作为面单元输出时生成      |
| `NFACE_n` Section          | `Base.Zone`，由完整 Section 名去掉最后一段得到 |

重名时从 `_0` 开始向当前名称追加数字后缀，直到名称唯一；名称返回顺序不构成接口保证。没有可展开 Section 的 Unstructured Zone
不生成占位组件。`GetComponent()` 返回该组件在扁平 `GetAllElement()` 结果中的 0-based 单元 ID，成功时替换输出数组。

`GetAllElement(std::vector<Elem>&)` 复制缓存中的 `Elem`；`GetAllElement(ElementTable&)` 将同一结果写入连续存储表，并可通过
`ElementView` 读取 `nodes`、`mid_nodes` 和 `corner_nodes` 三个范围。当前读取器为支持的扁平单元填充 `nodes`，其余两个范围保持为空。
`Elem::id` 按扁平输出顺序从 0 开始，与输出数组下标一致，不应直接视为 CGNS 原始元素号。`Elem::type` 是对应
`CG_ElementType_t` 的整数值。固定类型与 `MIXED` Section 按 Base/Zone/Section 遍历顺序展开，`Elem::npts` 与 `Elem::nodes`
分别给出节点数和节点 ID；节点 ID 加上被接受 Zone 的累计节点偏移后转换为全局 0-based 编号。

### 多面体展开

`fatten_section_elem_poly()` 对已经收集的 `NGON_n/NFACE_n` Section 进行两阶段展开：

1. 遍历全部 NGON Section，建立局部 `unordered_map<cgsize_t, FaceNodes>`。key 是 `section.range_start + 面在 Section 内的序号`
   ，即 CGNS 原始面元素号；value 是已经转换为全局 0-based 编号的面节点列表。面号不需要从 1 开始，多个 NGON Section
   的面号区间也不需要相邻。重复面号记录错误并保留先加入的面；空面或含非法顶点号的面跳过。
2. 遍历全部 NFACE Section，以 `cgsize_t` 读取带符号的面号，取绝对值后查 map。负面号只反转当前节点列表副本，不修改共享 NGON
   数据。面号唯一时，已收集 Section 的排列顺序和共享面的引用先后都不会改变面匹配或方向。

NFACE 面号为 0、为 `cgsize_t` 最小值或查不到对应 NGON
面时，记录警告并跳过该面。至少保留一个面就输出该单元；所有面均被跳过的单元不输出。这是局部容错行为，不包含多面体闭合性验证。过滤后的单元仍按扁平输出顺序重新编号，不保留原始
Section 元素号。

多面体结果的 `Elem::npts` 表示保留的面数，`Elem::nodes` 按 `[面节点数, 该面的节点ID..., 下一面节点数, ...]` 编码。例如
`npts = 2`、`nodes = [3, 0, 1, 2, 4, 2, 3, 4, 5]` 表示一个三节点面和一个四节点面；其中 `nodes[0]` 和 `nodes[4]`
是长度前缀，不能当成节点 ID。

内部参数 `separate_surface = true` 时，还按 NGON Section 顺序输出面单元并注册对应组件；为 `false` 时，NGON 仅用于面查找，输出单元来自
NFACE。若一个 Zone 只有 NGON 而没有 NFACE，则自动输出每个 NGON Section 的面单元，并将每个 Section 注册为对应组件。当前唯一调用点使用
`false`，公开 API 和命令行没有提供该开关，行为与 FlowSolution 的位置无关。函数结束时清空待处理的 `m_ngon_nface`。

### 当前网格读取限制

以下描述是当前实现的限制，调用方应结合返回结果与日志判断读取完整性：

- 纯 NGON Zone 会在多面体展开阶段输出 NGON 面单元和按 Section 注册的组件；NGON/NFACE Zone 则按上述两阶段流程输出 NFACE
  多面体单元。
- `Close()` 会清空节点、单元、组件、字段、待处理多面体 Section 及文件布局；初始化中途失败时，已经生成的其他缓存仍可能保留到下一次
  `Close()`。
- 初始化仅以是否读到 Base 判断最终成功，所有 Zone 均被跳过时也可能返回 `true`；`GetAllElement()` 和
  `GetAllComponentName()` 没有“最终结果非空”的额外要求。空单元/组件会触发再次初始化，而中途失败留下的非空缓存又可能在后续查询中被直接复用；当前不保证失败后重试的缓存一致性。
- 坐标描述读取失败、数据类型不受支持或 `cg_coord_read()` 失败时跳过 Zone。坐标按 CGNS 枚举顺序装入 x/y/z，未按坐标名称重排或转换坐标系。
- MIXED 当前按每条记录的类型调用 `cg_npe()` 核对节点数；非法类型号会回退为 `NODE`，节点数不匹配会使该 Section
  读取失败。不要将读取成功视为 MIXED 拓扑已完整验证。

整数结果使用 64 位 `ReaderAPI::Integer`（`std::int64_t`），内部 Section 范围、connectivity、面号和累计偏移使用 `cgsize_t`，当前 vendored CGNS
为 64 位尺寸构建。固定类型与 MIXED 检查 Section 数量和累计单元数量；NGON 检查顶点号为正且加节点偏移后的编号可由
`ReaderAPI::Integer` 表示；NFACE 检查当前扁平单元数量是否仍可表示，并在取绝对值前排除最小负值。NFACE 的内部面数计数仍使用
`int`，这些检查尚未完整覆盖其范围，也未完整校验 Zone 内顶点编号上界。

`GetAllNodeCoordinates()` 在按需初始化失败或累计节点数量超出 `ReaderAPI::Integer` 范围时返回 `false`
，调用方输出容器不变，但此前生成的内部缓存会保留。初始化完成后替换节点输出并返回 `true`，即使结果为空也不代表文件包含可用网格。同一
reader 的文件与数据读取接口不保证并发调用安全。

日志级别依次为 `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR` 和 `CRITICAL`。

### 场值读取

`ReaderApiTypes.hpp` 定义 `Integer = std::int64_t`、`Real = float`，以及 `Node` 和 `Field`；`Elem` 定义位于 `Types/ElementTypes.hpp`。
`Field::var` 是物理场组名，`Field::sub_vars` 是该组中的 CGNS 原始字段名。每个组只包含一种位置：`0` 为 `Vertex`，`1` 为 `CellCenter`。

首次字段查询构建布局，后续复用布局，但每次场值查询都会重新读取数据，不缓存数值。`Close()` 清除分组、编号偏移及初始化状态。
布局先按原始字段名收集两种位置的索引，再分别在每种位置内沿用名称前缀、下划线和 `Magnitude` 的分组规则。

同一物理量同时存在于两种位置时，组名分别加上 `_vertex` 和 `_cellcenter`；仅存在于一种位置时保留原组名。
子字段保留 CGNS 原名，不添加位置后缀。例如不同 Zone 中各有一个 FlowSolution，包含 `u_1(Vertex)`、`u_2(Vertex)`、
`u_1(CellCenter)` 和 `u_2(CellCenter)`，返回：

```text
u_vertex     [u_1, u_2]
u_cellcenter [u_1, u_2]
```

每种位置仅包含实际存在的分量，不用另一种位置的分量补齐。相同原始字段名、相同位置的数据按 Base/Zone 顺序合并。
生成的公开组名若与另一分组重名，则布局初始化返回 `false`；分组和偏移仅在整个布局构建成功后写入缓存。
没有可用字段时，`GetAllFieldFunctionName()` 返回 `false`，保留调用方原有输出。

`GetFieldFunctionPosition(var)` 仅按 `var` 分组查询统一位置，组内所有子字段共用该位置。
`Open()` 成功后即可直接查询，无需先调用 `GetAllFieldFunctionName()`；首次位置查询也会按需构建字段布局。
例如 `GetFieldFunctionPosition("u_vertex")` 返回 `0`，`GetFieldFunctionPosition("u_cellcenter")` 返回 `1`。
文件未打开、布局初始化失败或组名不存在时返回 `-1`。

`GetFieldFunctionIds(var, sub_var, ids)` 和 `GetFieldFunctionData(var, sub_var, data)` 先定位 `var` 分组，
再定位该组内的 `sub_var`。组名和子字段名区分大小写，调用方应使用枚举返回的名称。
错误组名或不属于该组的子字段不会回退到其他分组；ID 和数据查询返回 `false` 并保留调用方输出。
子字段查询失败不影响有效分组的位置查询结果。

字段读取按业务前提处理每个 Zone 最多一个 `FlowSolution_t`，当前访问第一个 FlowSolution，不提供多解或时间步选择。
无解的 Zone 跳过字段读取，但仍计入全局编号偏移。支持 Structured 和 Unstructured Zone，只接受 `Vertex` 和 `CellCenter`，
以及 `RealSingle` / `RealDouble` 字段；场值经 `cg_field_read(..., CG_RealSingle, ...)` 转换为单精度。
不提供 PointSet 映射、Rind 处理、面中心场、DiscreteData、ZoneSubRegion 或粒子场读取。

节点场和单元场分别按各 Zone 的 `VertexSize` 与 `CellSize` 累加编号偏移；某个 Zone 缺少该字段时不补值，因此一个字段的
ID 可以不连续。此编号独立于网格 Section 展开：网格读取跳过 Zone、包含边界面 Section 或过滤单元时，不能假定字段 ID 等于
`GetAllNodeCoordinates()` / `GetAllElement()` 结果下标，也不能普遍将单元场 ID 直接视为 `Elem::id`。

调用方按业务约定传入空的字段 ID 和场值输出数组，由 DLL 读取并填充；当前实现通过追加构建输出。
结果数组为空时返回 `false`。CGNS 字段描述或场值读取失败时，当前数据读取会跳过失败项，因此 `true` 不保证所有 Zone 的值均已读取，
调用方应核对 ID 与场值数量。

### 流体扩展

`GetFluidExtensions()` 是可选能力入口。`ReaderCGNS` 的 `CgnsCore` 持有一个内部 `FluidExtensions` 对象，该对象引用同一 reader 的
`ReaderFieldData` 和 `LogDispatcher`，复用字段布局、查询接口和日志回调。获取接口始终返回该对象的地址，无需先打开文件；其他 reader 可以保留默认的 `nullptr` 实现。

返回指针由 reader 拥有，调用方不能删除它。指针在 reader 销毁前保持有效，`Close()` 和后续 `Open()` 不改变其地址；扩展查询针对当前打开的文件。
销毁 reader 或卸载 DLL 前必须停止所有扩展调用。扩展没有独立的文件句柄，字段布局仍由 reader 在 `Close()` 时清理。

`HasVelocityField()` 遍历 `GetAllFieldFunctionName()` 返回的所有 `Field::sub_vars`，先将待匹配名称转为小写，再分别在编译期排序的 X、Y、Z 别名数组中按完整名称进行二分查找。
三个分量各自匹配到一个名称时返回 `true`；文件未打开、字段名称查询失败、结果为空或缺少任意分量时返回 `false`。

| 分量 | 接受的别名 |
|------|------------------------------|
| X | `velocityx`、`velocity_0`、`velocity[i]`、`ux`、`velocity_vectorsx` |
| Y | `velocityy`、`velocity_1`、`velocity[j]`、`uy`、`velocity_vectorsy` |
| Z | `velocityz`、`velocity_2`、`velocity[k]`、`uz`、`velocity_vectorsz` |

名称比较忽略大小写，其他字符须完整一致。例如 `VelocityX`、`VELOCITY_0` 和 `UX` 均可作为 X 分量。三个分量可以分别使用不同命名方式；
匹配时只转换名称副本，按 X/Y/Z 顺序记录每个分量的实际组名和原始子字段名，后续按这对名称严格查询位置、ID 和数据。
CGNS 读取诊断和扩展自身的错误都沿用 reader 的日志回调；重新注册或清除回调同样作用于扩展。布尔结果不区分读取失败和分量缺失。

`HasVelocityField()` 优先选择同一位置的完整 X/Y/Z 分量：两种位置均完整时优先 `Vertex`，否则选择完整的 `CellCenter`。
分量可以属于不同的物理场分组，以兼容 `VelocityX`、`UX` 及混合别名等命名方式。若三个名称齐全但分别位于不同位置，
仍返回 `true`，由 `GetVelocityFieldPosition()` 报告位置不一致；名称检测不验证 ID 覆盖或数值读取结果。
只有两个分量的二维速度场，以及原始字段名自带 `_Vertex` / `_CellCenter` 后缀的名称，不在当前三分量别名匹配范围内。
自动生成的位置后缀只出现在组名中，不影响子字段别名匹配。

`GetVelocityFieldPosition()` 独立检查分量是否存在，并要求三个分量的位置一致：`0` 为 `Vertex`，`1` 为 `CellCenter`。文件未打开、
缺少分量、查询失败或位置不一致时返回 `-1`；位置不一致的日志包含分量名、实际位置和预期位置。无需先调用 `HasVelocityField()`。

`GetVelocityField(data, ids)` 的两个参数均为输出引用。成功时替换 `data` 为三个分量数组，`data[0]`、`data[1]`、`data[2]` 分别对应
X、Y、Z；每个数组与 `ids` 长度相同，`data[component][i]` 是实体 `ids[i]` 上的对应速度分量。三个分量必须具有相同位置、相同且非空的
ID 序列，且各自的值数量等于 ID 数量；共同的 ID 可以不连续。缺少分量、读取失败或上述校验失败时返回 `false`，保留调用方原有的两个输出。
该接口复用现有字段读取行为，不提供额外的 Zone 完整性检查。

每次查询都会重新匹配当前文件的字段，并先清空上次的匹配结果；重复调用不会追加旧结果，文件切换后的查询也不会使用之前的分量名。
首次查询可能读取文件并构建字段布局。扩展遵循 reader 的数据查询并发约定，当前 DLL 内的 CGNS 文件访问应串行执行。

## 动态加载约定

ReaderCGNS 的交付物是仓库根目录 `ReaderAPI/` 下的完整公开头目录和 `ReaderCGNS.dll`，调用方不依赖 import library。DLL 只提供以下两个稳定名称，由调用方通过
`GetProcAddress` 解析：

- `CreateReaderCGNS`；
- `DestroyReaderCGNS`。

上述导出名称是 ReaderCGNS 与调用方之间的工程接口契约，不是实现细节。除非明确实施破坏性接口变更，否则不得改名、删除或复用于其他语义。确需调整时，必须在同一变更中同步更新
DLL 导出、调用方的 `GetProcAddress` 名称、相关测试和本文档，并保证配套产物一同交付。

`ReaderApiBase.h` 不声明需要 import library 的导出函数，而是提供两个工厂函数的指针类型。调用方使用这些类型解析导出、创建
reader，并通过 reader 虚接口完成文件操作和日志配置。头文件与 DLL 必须配套交付；这是工程交付约定，本项目不额外提供 ABI
版本导出或运行时版本校验。

公开整数类型为 64 位，影响 `Node`、`Elem`、`ElementTable` 及 ID 数组的二进制表示。使用旧版 32 位 `Integer` 头文件构建的调用方
必须重新编译，并与使用相同公开头文件构建的 `ReaderCGNS.dll` 配套交付。

新增 `GetFluidExtensions()` 及流体扩展虚接口会改变 C++ 虚表布局；`GetVelocityField()` 使用输出引用。已有调用方必须使用新的公开头文件
重新编译，并与同一版本的 `ReaderCGNS.dll` 配套交付。

工厂导出使用 C 符号名，但 reader 虚接口及 `std::string` / `std::vector` 参数仍是 C++ ABI。调用方应与 DLL 使用兼容的 MSVC
工具链、相同构建配置和运行库（Debug `/MDd`，Release `/MD`），并通过 `DestroyReaderCGNS` 销毁实例。

## 日志并发约定

每个 `ReaderAPI::ReaderApiBase` 实例独立维护一个回调和一个上下文指针。日志回调是可选能力；未注册时文件检查仍会执行，但不会向应用输出结构信息。注册状态遵循以下规则：

- 未绑定时，传入非空 callback 的 `SetLogCallback()` 完成绑定并返回 `true`；context 可以为 `nullptr`；
- `SetLogCallback(nullptr, context)` 返回 `false` 且不改变当前状态；
- 已绑定时再次调用 `SetLogCallback()` 返回 `false`，原 callback/context 保持不变；
- `ClearLogCallback()` 停止新的分发，等待已经进入的当前实例回调结束，然后清除绑定并返回 `true`；
- 未绑定时调用 `ClearLogCallback()` 是幂等操作并返回 `true`；
- 成功清理后可以再次注册；两个线程同时对同一 reader 注册时只有一个调用成功；
- 注册失败只通过返回值通知，DLL 不通过日志回调报告该错误，调用方可以使用自己的日志系统记录警告。

回调执行和数据生命周期遵循以下约定：

- 回调在触发日志的 ReaderCGNS 线程上同步执行；
- 同一个 callback 可能被多个 reader 或线程并发进入，调用方必须保证回调线程安全；
- `file` 保留编译器提供的原始源码路径，ReaderCGNS 不提取文件名，最终展示形式由调用方决定；
- `context`、`file` 和 `message` 都是借用数据，仅在本次回调期间有效；
- 在任意 ReaderCGNS 回调执行期间，从当前线程对任意 reader 调用 Set/Clear 都返回 `false` 且不改变目标实例状态；该规则优先于未绑定
  reader 的幂等 Clear；
- 回调抛出的异常会被库捕获，异常不会越过动态库边界传播。

上下文对象由调用方拥有，必须存活到成功的 `ClearLogCallback()` 或 `DestroyReaderCGNS()` 返回。销毁 reader 前，调用方必须停止该对象的所有
API 调用；dispatcher 的析构清理不提供与并发成员调用安全销毁的保证。日志 dispatcher 的并发保护也不代表底层 CGNS/HDF5
构建支持任意并发文件访问；并发读取策略仍应遵循所使用 CGNS 与 HDF5 库的线程安全配置。

当前 ReaderCGNS vendored HDF5 未启用 `H5_HAVE_THREADSAFE`，也未启用并行 HDF5；调用方应串行执行同一 DLL 内各 reader 的 CGNS
文件访问，不能仅靠为每个线程创建 reader 获得安全并发读取。

## 返回值与错误处理

`Open()` 会验证 CGNS 文件类型并以 `CG_MODE_READ` 打开文件，然后初始化 Base/Zone 布局。类型检查或文件打开失败时返回 `false`
；布局初始化失败时调用 `Close()` 后返回 `false`。版本或精度读取失败只记录日志，不阻止后续布局初始化。打开过程会记录只读打开尝试、可用的存储类型/版本/精度、Base
名称回退和最终成功状态。调用方应仅在 `Open()` 成功且 `IsOpen()` 为 `true` 时调用数据查询和 `info()`，并在结束后显式调用
`Close()`
。使用同一实例打开不同文件时，当前文件会先被关闭；再次打开同一路径且文件仍处于打开状态时直接返回成功。缓存清理的当前例外见[当前网格读取限制](#当前网格读取限制)。

`GetSolverType()` 只读取 Base/Zone 布局中第一个有效 `CGNSBase_t` 下直接声明的 `FlowEquationSet_t`，不遍历 Zone，也不根据
`SimulationType_t` 或其他节点推断方程类型。节点不存在或读取失败时，接口保留对应 CGNS 日志并返回 `"Unknown"`。

`info()` 没有返回值。节点级 CGNS API 错误不会汇总为调用结果，而是记录对应状态与 `cg_get_error()`
后在可行时继续。因此日志内容是判断局部读取问题的主要依据。`Close()` 同样没有返回值，关闭失败通过日志报告。

内部 `CGNS_LOG_CALL(expression)` 适配器只求值一次 CGNS API 表达式。状态为 `CG_OK` 时不输出；其他状态会连同状态名称或数值、调用表达式、
`cg_get_error()` 文本及调用位置交给日志回调，并原样返回状态码。它不会抛出异常、提前返回或改变调用方控制流；允许调用点忽略返回值，需要根据失败结果分支时也可以显式检查该返回值。

## 目录结构

```text
ReaderAPI/                         # 位于仓库根目录，与 ReaderCGNS/ 同级
├── ReaderApiBase.h                # reader 接口、工厂及日志协议类型
├── ReaderApiTypes.hpp             # Integer/Real 与 Node/Field
└── Types/
    └── ElementTypes.hpp           # Elem、ElementTable 与视图类型

ReaderCGNS/
├── CMakeLists.txt
├── Readme.md
├── CGNS.md                         # CGNS 文件格式与数据结构
├── CGNS_API.md                     # CGNS 4.5.1 C API 开发参考
├── src/
│   └── ExportFunctions.cpp         # Create/Destroy reader 导出
├── Common/
│   ├── CgnsTypes.hpp               # 内部 CGNS 名称长度常量
│   ├── CgnsTopology.hpp            # Base/Zone/Section 网格拓扑类型
│   └── CgnsFiled.hpp               # 内部字段类型
├── Core/
│   ├── CgnsCore.h                  # CGNS 层次遍历实现
│   ├── FileManager.h               # 文件生命周期、版本与 Base/Zone 布局
│   ├── ReaderMeshData.h            # Base/Zone/Section 网格拓扑初始化
│   ├── ReaderFieldData.h           # 字段布局、名称合并与单个/批量场值读取
│   ├── FluidExtensions.h           # 复用字段读取能力的流体扩展实现
│   └── src/
├── Utils/
│   ├── Logger.h                    # 实例 dispatcher、格式化与错误适配
│   └── src/Logger.cpp              # dispatcher 并发与生命周期实现
└── 3rdparty/
    └── cgns/                       # CGNS 静态库及其 CMake 配置
```

CGNS 层次、节点语义、元素类型和边界条件见 [`CGNS.md`](./CGNS.md)；仓库版本的完整 Mid-Level Library C API 见 [
`CGNS_API.md`](./CGNS_API.md)。

## 构建与链接

该模块使用根工程的输出目录和编译选项，将仓库根目录及模块内的 `Common/`、`Utils/`、`Core/` 加入私有 include 路径。CMake 通过模块内 `3rdparty/cgns/` 设置 `CGNS_ROOT` 查找 vendored 依赖，并为 HDF5 的包配置显式指定 Debug 的 `debug/lib/zsd.lib` 和 Release 的 `lib/zs.lib`。应从仓库根目录配置：

```powershell
cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64
cmake --build build/Debug --config Debug --target ReaderCGNS
cmake --build build/Debug --config Release --target ReaderCGNS
```

Windows 输出位于：

```text
bin/Debug/ReaderCGNS.dll
bin/Release/ReaderCGNS.dll
```

同一源码树中的调用目标应将仓库根目录加入 include 路径，以使用 `#include "ReaderAPI/ReaderApiBase.h"`；如需保证构建顺序，可添加 target 依赖，但不要链接 ReaderCGNS import library：

```cmake
target_include_directories(YourTarget PRIVATE
        "${CMAKE_SOURCE_DIR}"
)
add_dependencies(YourTarget ReaderCGNS)
```

`ReaderCGNS` 使用 CMake `MODULE` 库类型构建，CGNS 依赖以 `CGNS::cgns_static` 私有链接，MSVC 下使用 `/NOIMPLIB` 禁止生成 import library。调用方应配套分发 `ReaderAPI/` 完整目录（包括 `Types/` 子目录）和 DLL，并将包含 `ReaderAPI/` 的父目录加入 include 路径。模块内部头文件位于 `ReaderCGNS/Core/`、`ReaderCGNS/Common/` 和 `ReaderCGNS/Utils/`，不属于公开接口。

### 回归测试

Windows 下启用 `BUILD_TESTING`（默认开启）时，`ReaderCGNS/tests/` 注册元数据、速度数据、别名匹配和扩展日志四组 CTest 测试。测试使用 vendored CGNS
生成临时文件，再通过公开工厂动态加载实际构建的 `ReaderCGNS.dll`；临时文件在测试结束时删除。覆盖重复查询、文件切换、节点/单元位置、
跨 Zone 的不连续 ID、分量不一致、失败时保留输出、所有别名的大小写混用，以及 reader 日志回调的隔离、清除和重新注册。

```powershell
cmake --build build/Debug --config Debug --target ReaderCGNSFluidExtensionsTests
ctest --test-dir build/Debug -C Debug -R '^ReaderCGNS\.FluidExtensions\.' --output-on-failure
```

Release 验证使用相同命令并将两处配置参数 `Debug` 改为 `Release`，构建目录仍为 `build/Debug`。

## 开发约定

- 对外兼容面仅包含仓库根目录 `ReaderAPI/` 下的头文件和导出符号；
- 新增公开 API 时，同时说明所有权、线程安全、错误与生命周期语义；
- 保持 CGNS 文件只读，除非通过独立设计明确引入写入接口；
- 新增遍历节点时，使用统一日志层级并保留 CGNS 错误上下文；
- 不在共享库内部绑定 spdlog 等应用日志框架，日志后端由调用方决定；
- 第三方内容位于 `3rdparty/cgns/`，只在有计划的依赖升级中修改。
