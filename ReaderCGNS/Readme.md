# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 动态加载模块。它基于官方 CGNS Mid-Level Library 遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出，并提供名称列表、节点坐标、单元连接和场值查询。

当前公开能力定位为“结构检查、诊断与数据查询”：接口可读取文件版本和 Base 级方程类型、列出 element set 与场函数名称，以扁平数组返回节点坐标和受支持单元的连接关系，并读取满足下述限制的节点/单元中心场值；`ReaderAPI::ReaderApiBase::info()` 负责输出更完整的元数据和连接摘要。

## 能力范围

当前检查流程覆盖：

- CGNS 存储类型、文件版本与数据精度；
- Base、Zone、迭代信息与结构化/非结构化尺寸；
- 首次查询 element set 名称、节点坐标或单元连接时按需构建 Base/Zone/Section 内部网格拓扑，包括坐标、Structured connectivity、固定单元连接表以及 `MIXED/NGON_n/NFACE_n` 变长连接表；
- FlowSolution、DiscreteData 与 ZoneSubRegion；
- 按名称读取单个或批量 FlowSolution 字段值，支持 `Vertex` 和 `CellCenter`；
- GridCoordinates 与元素 Section/Connectivity；
- 一对一连接、一般网格连接与 Overset Holes；
- BoundaryCondition 及其 DataSet；
- 刚体和任意网格运动；
- ParticleZone、粒子坐标与粒子解描述。

输入文件使用 `CG_MODE_READ` 打开。库不会修改 CGNS 文件，也不会主动创建日志文件；诊断信息交给调用方注册的回调，查询数据通过返回值和输出参数交付。

### 内部网格拓扑

`FileManager` 在 `Open()` 期间通过 `initialize_base_zone_layout()` 确定本次需要分析的 Base/Zone 位置，跳过 Zone 数量读取失败或没有普通 Zone 的 Base；因此仅含 ParticleZone 的文件无法成功打开。首次调用 `GetAllElementSetName()`、`GetAllNodeCoordinates()` 或 `GetAllElement()` 时，`ReaderMeshData` 遍历这组索引，读取临时 Base/Zone/Section 数据，并缓存合并后的节点坐标、展开单元与集合映射。`ReaderFieldData` 使用独立的按需缓存保存 FlowSolution 字段布局，查询场值时直接读取文件，不依赖网格拓扑缓存。当前网格拓扑不缓存边界条件或 Zone 间连接数据。

Structured Zone 不要求存在 `Elements_t`；`ReaderMeshData` 根据 `VertexSize` 和 `CellSize` 合成一个 Section，并按维度展开为 `BAR_2`、`QUAD_4` 或 `HEXA_8` 的 1-based connectivity。Unstructured Zone 会遍历 Section：固定元素类型通过 `cg_npe()` 校验每个元素的节点数并读取连续 connectivity；`MIXED`、`NGON_n` 和 `NFACE_n` 通过 `cg_poly_elements_read()` 同时保存 connectivity 与 `ElementStartOffset`。Section 声明 parent data 时只记录存在标志，不缓存 `ParentElements` 或 `ParentElementsPosition` 的原始数据。

`read_section_topology()` 统一验证 Section 元素范围为正且起止有序，并检查数量和数据长度能否由 `size_t` 表示。对于变长 Section，先检查数量上限，再分配 connectivity 和 `ElementSize + 1` 个 offsets；读取成功后检查 offsets 首项为 0、末项等于 connectivity 长度且单调不减。只有通过这些检查的 Section 才会交给展开逻辑。`fatten_section_elem_poly()` 依赖该前置条件，不重复检查每条记录的 offsets 范围，保留依赖当前节点/单元偏移和面引用的检查。

网格初始化在遍历期间直接向成员缓存追加结果。Base 读取失败会返回 `false`，Zone 或 Section 读取失败通常记录日志并跳过；发生中途失败时，已经生成的内部缓存不会自动回滚。`Open()` 只负责文件和 Base/Zone 布局初始化。节点和单元查询以单元缓存是否为空决定是否初始化，集合查询以集合映射是否为空决定是否初始化；当前没有独立的“初始化已完成”标志。重复查询通常复用缓存，失败后重试和空结果的行为见下文[当前网格读取限制](#当前网格读取限制)。

## 公开接口

公开头文件位于 `include/ReaderAPI/`：

```cpp
#include "ReaderAPI/ReaderApiBase.h"
```

该头文件同时提供 `ReaderAPI::ReaderApiBase`、reader 工厂函数指针类型，以及
`ReaderAPI::Logger::LogLevel` 和 `LogCallback` 日志协议类型。

| API | 作用 |
|---|---|
| `ReaderAPI::ReaderApiBase::SetLogCallback(callback, context)` | 为当前 reader 注册日志回调和可选上下文。 |
| `ReaderAPI::ReaderApiBase::ClearLogCallback()` | 清除当前 reader 的日志回调。 |
| `ReaderAPI::ReaderApiBase::Open(path)` | 以只读方式打开 CGNS 文件。 |
| `ReaderAPI::ReaderApiBase::Close()` | 关闭当前文件。 |
| `ReaderAPI::ReaderApiBase::IsOpen()` | 查询文件是否已打开。 |
| `ReaderAPI::ReaderApiBase::GetVersion()` | 返回当前文件记录的 CGNS 版本。 |
| `ReaderAPI::ReaderApiBase::GetSolverType()` | 返回 Base/Zone 布局中第一个有效 Base 下 `FlowEquationSet_t/GoverningEquations_t` 的类型名称。 |
| `ReaderAPI::ReaderApiBase::GetAllElementSetName(names)` | 将当前文件的 element set 名称追加到调用方提供的字符串数组。 |
| `ReaderAPI::ReaderApiBase::GetAllFieldFunctionName(names)` | 将当前文件的场函数名称追加到调用方提供的字符串数组。 |
| `ReaderAPI::ReaderApiBase::GetAllNodeCoordinates(nodes)` | 将可读 Zone 的节点坐标追加到调用方提供的节点数组。 |
| `ReaderAPI::ReaderApiBase::GetAllElement(elements)` | 将缓存中的单元类型和连接数据复制追加到调用方提供的单元数组。 |
| `ReaderAPI::ReaderApiBase::GetFieldFunctionData(name, field)` | 按公开字段名读取场值，成功时替换输出 `Field`。 |
| `ReaderAPI::ReaderApiBase::GetFieldFunctionData(names, fields)` | 按请求顺序批量读取场值，至少一项成功时用成功项替换输出数组。 |
| `ReaderAPI::ReaderApiBase::info()` | 遍历当前已打开文件并输出结构检查信息。 |

所有数据查询都要求文件已成功打开。四个 `GetAll*` 容器输出接口成功时追加结果且不会预先清空容器，输出容器由调用方拥有；按需初始化返回 `false` 时保留调用方容器原内容。缓存复用期间，重复向同一容器查询会追加同一批数据及其原有 ID，不按容器已有大小重新编号，需要替换结果时由调用方先清空容器。`GetAllFieldFunctionName()` 返回去重后的公开字段名称，顺序不构成接口保证。`GetAllNodeCoordinates()` 追加缓存中的 `Real` 坐标；正常首次构建时，`Node::id` 从 0 开始按被接受的 Zone 累加生成。两个 `GetFieldFunctionData()` 重载使用替换语义，详见下文。

element set 的当前命名如下：

| 来源 | 集合名称 |
|---|---|
| Structured Zone | `Base.Zone` |
| 固定类型或 `MIXED` Section | `Base.Zone.Section` |
| `NGON_n` Section | `Base.Zone.Section`，仅在内部启用 `separate_surface` 时生成 |
| `NFACE_n` Section | `Base.Zone`，由完整 Section 名去掉最后一段得到 |

重名时从 `_0` 开始向当前名称追加数字后缀，直到名称唯一；名称返回顺序不构成接口保证。没有可展开 Section 的 Unstructured Zone 不生成占位集合。公开 API 当前只提供集合名称查询。

`GetAllElement()` 复制缓存中的 `Elem`。`Elem::id` 是用于唯一标识单元的属性，不承诺连续、排序或等于输出数组下标，也不应直接视为 CGNS 原始元素号。`Elem::type` 是对应 `CG_ElementType_t` 的整数值。固定类型与 `MIXED` Section 按 Base/Zone/Section 遍历顺序展开，`Elem::npts` 与 `Elem::nodes` 分别给出节点数和节点 ID；节点 ID 加上被接受 Zone 的累计节点偏移后转换为全局 0-based 编号。

### 多面体展开

`fatten_section_elem_poly()` 对已经收集的 `NGON_n/NFACE_n` Section 进行两阶段展开：

1. 遍历全部 NGON Section，建立局部 `unordered_map<cgsize_t, FaceNodes>`。key 是 `section.range_start + 面在 Section 内的序号`，即 CGNS 原始面元素号；value 是已经转换为全局 0-based 编号的面节点列表。面号不需要从 1 开始，多个 NGON Section 的面号区间也不需要相邻。重复面号记录错误并保留先加入的面；空面或含非法顶点号的面跳过。
2. 遍历全部 NFACE Section，以 `cgsize_t` 读取带符号的面号，取绝对值后查 map。负面号只反转当前节点列表副本，不修改共享 NGON 数据。面号唯一时，已收集 Section 的排列顺序和共享面的引用先后都不会改变面匹配或方向。

NFACE 面号为 0、为 `cgsize_t` 最小值或查不到对应 NGON 面时，记录警告并跳过该面。至少保留一个面就输出该单元；所有面均被跳过的单元不输出。这是局部容错行为，不包含多面体闭合性验证。NFACE 路径仍按 Section 原始单元数量累计偏移，过滤单元后不压缩该偏移。

多面体结果的 `Elem::npts` 表示保留的面数，`Elem::nodes` 按 `[面节点数, 该面的节点ID..., 下一面节点数, ...]` 编码。例如 `npts = 2`、`nodes = [3, 0, 1, 2, 4, 2, 3, 4, 5]` 表示一个三节点面和一个四节点面；其中 `nodes[0]` 和 `nodes[4]` 是长度前缀，不能当成节点 ID。

内部参数 `separate_surface = true` 时，还按 NGON Section 顺序输出面单元并注册对应集合；为 `false` 时，NGON 仅用于面查找，输出单元来自 NFACE。当前唯一调用点使用 `false`，公开 API 和命令行没有提供该开关，行为与 FlowSolution 的位置无关。函数结束时清空待处理的 `m_ngon_nface`。

### 当前网格读取限制

以下描述是当前实现的限制，调用方应结合返回结果与日志判断读取完整性：

- 纯 NGON/NFACE Zone 的调用路径尚未完整接通：`read_section_topology()` 将这些 Section 加入待处理列表后返回 `false`，`read_unstructured_zone_sections()` 在没有成功展开的固定类型或 MIXED Section 时也返回 `false`。外层会跳过该 Zone 的节点追加与多面体展开。因此上述两阶段算法的能力不等于公开 API 已完整支持纯多面体文件。
- 被跳过 Zone 的待处理 Section 可能留到后续 Zone；`Close()` 会清空节点、单元、集合、字段及文件布局，但当前 `clear_grid_topology()` 未清除 `m_ngon_nface`。异常流程中的待处理数据可能跨 Zone 或跨文件残留，不能依赖关闭重开来完整复位这一状态。
- 初始化仅以是否读到 Base 判断最终成功，所有 Zone 均被跳过时也可能返回 `true`；`GetAllElement()` 和 `GetAllElementSetName()` 没有“最终结果非空”的额外要求。空单元/集合会触发再次初始化，而中途失败留下的非空缓存又可能在后续查询中被直接复用；当前不保证失败后重试的缓存一致性。
- 坐标描述读取失败或数据类型不受支持时跳过 Zone，但 `cg_coord_read()` 的失败状态目前只记日志，未使坐标读取立即失败。坐标按 CGNS 枚举顺序装入 x/y/z，未按坐标名称重排或转换坐标系。
- MIXED 当前以 offsets 差值减去类型字段得到节点数，未逐单元调用 `cg_npe()` 核对数量；非法类型号会回退为 `NODE`。不要将读取成功视为 MIXED 拓扑已完整验证。

整数结果使用 32 位 `ReaderAPI::Integer`，内部 Section 范围、connectivity、面号和累计偏移使用 `cgsize_t`，当前 vendored CGNS 为 64 位尺寸构建。固定类型与 MIXED 检查 Section 数量和累计单元数量；NGON 检查顶点号为正且加节点偏移后的编号可由 `ReaderAPI::Integer` 表示；NFACE 检查累计单元偏移的 `cgsize_t` 溢出，并在取绝对值前排除最小负值。这些检查尚未完整覆盖输出 `Elem::id`、`npts` 的 32 位转换，也未完整校验 Zone 内顶点编号上界。

`GetAllNodeCoordinates()` 在按需初始化失败或累计节点数量超出 `ReaderAPI::Integer` 范围时返回 `false`，调用方输出容器不变，但此前生成的内部缓存会保留。初始化完成后追加节点并返回 `true`，即使输出容器为空也只记录警告。同一 reader 的文件与数据读取接口不保证并发调用安全。

日志级别依次为 `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR` 和 `CRITICAL`。

### 场值读取

`ReaderApiTypes.hpp` 定义 `Integer = std::int32_t`、`Real = float`，以及 `Node`、`Elem` 和 `Field`。`Field` 包含以下成员：

| 成员 | 当前返回语义 |
|---|---|
| `name` | 查询使用的公开字段名。 |
| `type` | `0` 为节点场（`Vertex`），`1` 为单元中心场（`CellCenter`）；头文件注释中的 `2`（FaceCenter）和 `3`（PointSet）当前未实现读取。 |
| `ids` | 全局 0-based 实体编号，与 `values` 一一对应。 |
| `values` | 通过 `cg_field_read(..., CG_RealSingle, ...)` 转换得到的单精度值。 |
| `isEmpty()` | 名称、ID 或值数组为空，或两个数组长度不一致时返回 `true`。 |

首次名称或场值查询构建字段布局，后续复用布局，但每次场值查询都会重新读取数据，不缓存数值。`Close()` 清除布局。

当前字段布局有以下限制，任一有效 Base/Zone 中的布局错误都会使本次初始化失败：

- 支持 Structured 和 Unstructured Zone，节点及单元尺寸必须为正，维数和累计数量须通过范围检查。
- 每个 Zone 至多一个 `FlowSolution_t`；没有解的 Zone 跳过字段读取，但仍计入全局编号偏移。多个 FlowSolution 会返回 `false`，不自动选择时间步。
- 只接受 `Vertex` 和 `CellCenter`，解数组的值数量必须等于对应 Zone 的节点数或单元数。不提供 PointSet 映射、Rind 处理或面中心场读取；`DiscreteData`、ZoneSubRegion 和粒子场不在此接口范围内。
- 全文件无可用字段、字段布局读取失败、公开名称冲突或字段 ID 超出 32 位范围时，名称查询和依赖该布局的场值查询返回 `false`。

相同源字段名、相同位置的数据按 Base/Zone 顺序合并；若同一源名称跨 Zone 同时出现在节点和单元中心，则公开名分别为 `<原名>_Vertex` 与 `<原名>_CellCenter`，例如 `Pressure_Vertex`、`Pressure_CellCenter`。后缀名与其他字段原名冲突时初始化失败。调用方应使用 `GetAllFieldFunctionName()` 返回的名称查询。

节点场和单元场分别按布局中各 Zone 的 `VertexSize` 与 `CellSize` 累加编号偏移；某个 Zone 缺少该字段时不补值，因此一个字段的 ID 可以不连续。此编号独立于网格 Section 展开：网格读取跳过 Zone、包含边界面 Section 或过滤单元时，不能假定字段 ID 等于 `GetAllNodeCoordinates()` / `GetAllElement()` 结果下标，也不能普遍将单元场 ID 直接视为 `Elem::id`。

单字段重载在完整读取成功后替换输出对象；未知名称或读取失败返回 `false`，保留原输出。批量重载按请求顺序逐项读取，跳过失败项，不去重；至少一项成功便返回 `true` 并替换整个输出数组，全部失败或请求为空时返回 `false` 且保留原输出。批量成功不表示所有请求均成功，应核对返回的 `Field::name`。

## 动态加载约定

ReaderCGNS 的交付物是 `include/ReaderAPI/` 下的公开头和 `ReaderCGNS.dll`，调用方不依赖 import library。DLL 只提供以下两个稳定名称，由调用方通过 `GetProcAddress` 解析：

- `CreateReaderCGNS`；
- `DestroyReaderCGNS`。

上述导出名称是 ReaderCGNS 与调用方之间的工程接口契约，不是实现细节。除非明确实施破坏性接口变更，否则不得改名、删除或复用于其他语义。确需调整时，必须在同一变更中同步更新 DLL 导出、调用方的 `GetProcAddress` 名称、相关测试和本文档，并保证配套产物一同交付。

`ReaderApiBase.h` 不声明需要 import library 的导出函数，而是提供两个工厂函数的指针类型。调用方使用这些类型解析导出、创建 reader，并通过 reader 虚接口完成文件操作和日志配置。头文件与 DLL 必须配套交付；这是工程交付约定，本项目不额外提供 ABI 版本导出或运行时版本校验。

工厂导出使用 C 符号名，但 reader 虚接口及 `std::string` / `std::vector` 参数仍是 C++ ABI。调用方应与 DLL 使用兼容的 MSVC 工具链、相同构建配置和运行库（Debug `/MDd`，Release `/MD`），并通过 `DestroyReaderCGNS` 销毁实例。

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
- 在任意 ReaderCGNS 回调执行期间，从当前线程对任意 reader 调用 Set/Clear 都返回 `false` 且不改变目标实例状态；该规则优先于未绑定 reader 的幂等 Clear；
- 回调抛出的异常会被库捕获，异常不会越过动态库边界传播。

上下文对象由调用方拥有，必须存活到成功的 `ClearLogCallback()` 或 `DestroyReaderCGNS()` 返回。销毁 reader 前，调用方必须停止该对象的所有 API 调用；dispatcher 的析构清理不提供与并发成员调用安全销毁的保证。日志 dispatcher 的并发保护也不代表底层 CGNS/HDF5 构建支持任意并发文件访问；并发读取策略仍应遵循所使用 CGNS 与 HDF5 库的线程安全配置。

当前 ReaderCGNS vendored HDF5 未启用 `H5_HAVE_THREADSAFE`，也未启用并行 HDF5；调用方应串行执行同一 DLL 内各 reader 的 CGNS 文件访问，不能仅靠为每个线程创建 reader 获得安全并发读取。

## 返回值与错误处理

`Open()` 会验证 CGNS 文件类型并以 `CG_MODE_READ` 打开文件，然后初始化 Base/Zone 布局。类型检查或文件打开失败时返回 `false`；布局初始化失败时调用 `Close()` 后返回 `false`。版本或精度读取失败只记录日志，不阻止后续布局初始化。打开过程会记录只读打开尝试、可用的存储类型/版本/精度、Base 名称回退和最终成功状态。调用方应仅在 `Open()` 成功且 `IsOpen()` 为 `true` 时调用数据查询和 `info()`，并在结束后显式调用 `Close()`。使用同一实例打开不同文件时，当前文件会先被关闭；再次打开同一路径且文件仍处于打开状态时直接返回成功。缓存清理的当前例外见[当前网格读取限制](#当前网格读取限制)。

`GetSolverType()` 只读取 Base/Zone 布局中第一个有效 `CGNSBase_t` 下直接声明的 `FlowEquationSet_t`，不遍历 Zone，也不根据 `SimulationType_t` 或其他节点推断方程类型。节点不存在或读取失败时，接口保留对应 CGNS 日志并返回 `"Unknown"`。

`info()` 没有返回值。节点级 CGNS API 错误不会汇总为调用结果，而是记录对应状态与 `cg_get_error()` 后在可行时继续。因此日志内容是判断局部读取问题的主要依据。`Close()` 同样没有返回值，关闭失败通过日志报告。

内部 `CGNS_LOG_CALL(expression)` 适配器只求值一次 CGNS API 表达式。状态为 `CG_OK` 时不输出；其他状态会连同状态名称或数值、调用表达式、`cg_get_error()` 文本及调用位置交给日志回调，并原样返回状态码。它不会抛出异常、提前返回或改变调用方控制流；允许调用点忽略返回值，需要根据失败结果分支时也可以显式检查该返回值。

## 目录结构

```text
ReaderCGNS/
├── CMakeLists.txt
├── Readme.md
├── CGNS.md                         # CGNS 文件格式与数据结构
├── CGNS_API.md                     # CGNS 4.5.1 C API 开发参考
├── include/ReaderAPI/
│   ├── ReaderApiBase.h             # reader 接口、工厂及日志协议类型
│   └── ReaderApiTypes.hpp          # Integer/Real 与 Node/Elem/Field
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
│   └── src/
├── Utils/
│   ├── Logger.h                    # 实例 dispatcher、格式化与错误适配
│   └── src/Logger.cpp              # dispatcher 并发与生命周期实现
└── 3rdparty/
    └── cgns/                       # CGNS 静态库及其 CMake 配置
```

CGNS 层次、节点语义、元素类型和边界条件见 [`CGNS.md`](./CGNS.md)；仓库版本的完整 Mid-Level Library C API 见 [`CGNS_API.md`](./CGNS_API.md)。

## 构建与链接

该模块使用根工程的输出目录、编译选项和源码根路径，并通过模块内 `3rdparty/cgns` 查找 vendored 依赖，应从仓库根目录配置：

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

同一源码树中的调用目标应显式使用公开 include 路径；如需保证构建顺序，可添加 target 依赖，但不要链接 ReaderCGNS import library：

```cmake
target_include_directories(YourTarget PRIVATE
        ${CMAKE_SOURCE_DIR}/ReaderCGNS/include
)
add_dependencies(YourTarget ReaderCGNS)
```

`ReaderCGNS` 以运行时加载模块形式构建，CGNS 依赖以 `CGNS::cgns_static` 私有链接。该目标不提供 import library；调用方只应分发公开头和 DLL，并且不应直接包含 `Core/` 或 `Utils/` 下的内部头文件。

## 开发约定

- 对外兼容面仅包含 `include/ReaderAPI/` 下的头文件和导出符号；
- 新增公开 API 时，同时说明所有权、线程安全、错误与生命周期语义；
- 保持 CGNS 文件只读，除非通过独立设计明确引入写入接口；
- 新增遍历节点时，使用统一日志层级并保留 CGNS 错误上下文；
- 不在共享库内部绑定 spdlog 等应用日志框架，日志后端由调用方决定；
- 第三方内容位于 `3rdparty/cgns/`，只在有计划的依赖升级中修改。
