# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 动态加载模块。它基于官方 CGNS Mid-Level Library 遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出，并提供名称列表、节点坐标和单元连接查询。

当前公开能力定位为“结构检查、诊断与轻量数据查询”，而不是完整的数据导入器：接口可读取文件版本和 Base 级方程类型、列出 element set 与场函数名称，并以扁平数组返回节点坐标和受支持单元的连接关系；`ReaderAPI::ReaderApiBase::info()` 负责输出更完整的元数据和连接摘要。

## 能力范围

当前检查流程覆盖：

- CGNS 存储类型、文件版本与数据精度；
- Base、Zone、迭代信息与结构化/非结构化尺寸；
- 首次查询 element set 名称、节点坐标或单元连接时按需构建 Base/Zone/Section 内部网格拓扑，包括坐标、Structured connectivity、固定单元连接表以及 `MIXED/NGON_n/NFACE_n` 变长连接表；
- FlowSolution、DiscreteData 与 ZoneSubRegion；
- GridCoordinates 与元素 Section/Connectivity；
- 一对一连接、一般网格连接与 Overset Holes；
- BoundaryCondition 及其 DataSet；
- 刚体和任意网格运动；
- ParticleZone、粒子坐标与粒子解描述。

输入文件使用 `CG_MODE_READ` 打开。库不会修改 CGNS 文件，也不会主动创建日志文件；所有可观测信息均交给调用方注册的回调。

### 内部网格拓扑

`FileManager` 在 `Open()` 期间通过 `initialize_base_zone_layout()` 确定本次需要分析的 Base/Zone 位置。首次调用 `GetAllElementSetName()`、`GetAllNodeCoordinates()` 或 `GetAllElement()` 时，`ReaderMeshData` 只遍历这组索引并按需缓存 Base、Zone、Section 和各 Zone 的坐标，不会重新扫描已经被布局阶段过滤的节点。`GetAllFieldFunctionName()` 使用独立的按需缓存遍历各 Zone 的 FlowSolution 与字段描述。当前内部拓扑不读取场值、边界条件或 Zone 间连接数据。

Structured Zone 不要求存在 `Elements_t`；`ReaderMeshData` 根据 `VertexSize` 和 `CellSize` 合成一个 Section，并按维度展开为 `BAR_2`、`QUAD_4` 或 `HEXA_8` 的 1-based connectivity。Unstructured Zone 会遍历 Section：固定元素类型通过 `cg_npe()` 校验每个元素的节点数并读取连续 connectivity；`MIXED`、`NGON_n` 和 `NFACE_n` 通过 `cg_poly_elements_read()` 同时保存 connectivity 与 `ElementStartOffset`。Section 声明 parent data 时只记录存在标志，不缓存 `ParentElements` 或 `ParentElementsPosition` 的原始数据。

拓扑读取先构建临时结果；无法读取的 Base、Zone 或 Section 会记录错误并跳过，至少得到一个可读 Base 后才替换当前快照。坐标描述读取失败或坐标数据类型不受支持时，对应 Zone 不会进入可用拓扑。`Open()` 只负责文件和 Base/Zone 布局初始化，按需初始化失败由发起查询的接口返回 `false`。首次查询单元或 element set 名称时共同构建单元连接与集合缓存，成功后重复查询复用缓存。`Close()` 会关闭 CGNS 文件并释放网格拓扑、单元、集合与字段缓存。公开接口从缓存生成名称列表、扁平节点坐标和受支持单元的扁平连接数据；Base、Zone 与 Section 层次结构仍属于 DLL 内部实现。

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
| `ReaderAPI::ReaderApiBase::GetSolverType()` | 返回第一个 Base 下 `FlowEquationSet_t/GoverningEquations_t` 的类型名称。 |
| `ReaderAPI::ReaderApiBase::GetAllElementSetName(names)` | 将当前文件的 element set 名称追加到调用方提供的字符串数组。 |
| `ReaderAPI::ReaderApiBase::GetAllFieldFunctionName(names)` | 将当前文件的场函数名称追加到调用方提供的字符串数组。 |
| `ReaderAPI::ReaderApiBase::GetAllNodeCoordinates(nodes)` | 将可读 Zone 的节点坐标追加到调用方提供的节点数组。 |
| `ReaderAPI::ReaderApiBase::GetAllElement(elements)` | 将缓存中的单元类型和连接数据复制追加到调用方提供的单元数组。 |
| `ReaderAPI::ReaderApiBase::info()` | 遍历当前已打开文件并输出结构检查信息。 |

上述四个容器输出接口都要求文件已成功打开，输出容器由调用方拥有，成功时追加结果且不会预先清空容器。`GetAllElement()` 返回 `false` 时保留容器原内容；重复向同一容器查询会追加同一批单元及其原有 ID，不按容器已有大小重新编号，需要替换结果时由调用方先清空容器。`GetAllFieldFunctionName()` 返回去重后的字段名称，顺序不构成接口保证。`GetAllNodeCoordinates()` 按缓存中的 Base/Zone 顺序追加 `Real` 坐标，生成的 `Node::id` 在每次调用内从 0 连续编号。

element set 统一使用 `Base.Zone.Section` 命名，包括注册的 `NGON_n` 和 `NFACE_n` 集合；Structured Zone 合成的 Section 与 Zone 同名，因此名称为 `Base.Zone.Zone`。重名时从 `_0` 开始向当前名称追加数字后缀，直到名称唯一；名称返回顺序不构成接口保证。没有可展开 Section 的 Unstructured Zone 不生成占位集合。

`GetAllElement()` 复制缓存中的 `Elem`。固定类型与 `MIXED` Section 按缓存中的 Base/Zone/Section 顺序展开；`Elem::id` 从累计单元偏移连续编号，跳过的单元不占编号，`Elem::type` 是对应 `CG_ElementType_t` 的整数值，`Elem::npts` 与 `Elem::nodes` 分别给出节点数和节点 ID。节点 ID 按所有可读 Zone 合并为全局 0-based 编号。

遇到 `NGON_n` 或 `NFACE_n` 时会进入 Zone 级联合处理路径。`NGON_n` 使用面节点列表；`NFACE_n` 的连接按引用面展开，`npts` 表示保留的面数，`nodes` 使用“面节点数、该面的节点 ID、下一面节点数、……”的布局。该路径会按 Section 预分配单元 ID，过滤失败的 `NFACE_n` 后不重新编号，因此不能假定 ID 连续或等于输出数组下标。多面体路径仍有实现限制，不保证所有 Section 排列和面引用都能完整展开，应结合日志检查结果。

单元展开采用局部失败后继续处理的方式：`MIXED` 中节点数查询失败、节点数无效或与连接偏移长度不匹配的单元会记录错误并跳过，继续处理后续单元。首次构建要求单元数组和集合映射均非空，成功后保存单元缓存；`GetAllElement()` 将缓存追加到输出容器并返回 `true`，因此成功不代表所有 Section 和单元都已返回。按需初始化失败或最终单元数组、集合映射任一为空时返回 `false`，保留调用方容器原内容。

整数结果使用 32 位 `ReaderAPI::Integer`，内部累计节点/单元偏移与 Section 初始化返回的数量使用 `cgsize_t`。单元展开保留 Section 单元数量与累计单元数量的范围检查，超出范围时跳过该 Section；节点偏移和 connectivity 节点 ID 直接转换，不再做范围检查，输入需使用有效的 Zone 内节点编号，且累计节点数量与转换后的编号须在 `ReaderAPI::Integer` 表示范围内。

`GetAllNodeCoordinates()` 在按需初始化失败或累计节点数量超出 `ReaderAPI::Integer` 范围时返回 `false`；范围检查失败前已追加的节点会保留。完成遍历后返回 `true`，即使输出容器为空也只记录警告。同一 reader 的文件与数据读取接口不保证并发调用安全。

日志级别依次为 `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR` 和 `CRITICAL`。

## 动态加载约定

ReaderCGNS 的交付物是 `include/ReaderAPI/` 下的公开头和 `ReaderCGNS.dll`，调用方不依赖 import library。DLL 只提供以下两个稳定名称，由调用方通过 `GetProcAddress` 解析：

- `CreateReaderCGNS`；
- `DestroyReaderCGNS`。

上述导出名称是 ReaderCGNS 与调用方之间的工程接口契约，不是实现细节。除非明确实施破坏性接口变更，否则不得改名、删除或复用于其他语义。确需调整时，必须在同一变更中同步更新 DLL 导出、调用方的 `GetProcAddress` 名称、相关测试和本文档，并保证配套产物一同交付。

`ReaderApiBase.h` 不声明需要 import library 的导出函数，而是提供两个工厂函数的指针类型。调用方使用这些类型解析导出、创建 reader，并通过 reader 虚接口完成文件操作和日志配置。头文件与 DLL 必须配套交付；这是工程交付约定，本项目不额外提供 ABI 版本导出或运行时版本校验。

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

## 返回值与错误处理

`Open()` 会验证 CGNS 文件类型并以 `CG_MODE_READ` 打开文件，然后初始化 Base/Zone 布局；任一步失败都会关闭文件、清理已构建数据并返回 `false`。打开过程会记录只读打开尝试、可用的存储类型/版本/精度、Base 名称回退和最终成功状态。调用方应仅在 `Open()` 成功且 `IsOpen()` 为 `true` 时调用数据查询和 `info()`，并在结束后显式调用 `Close()`。使用同一实例打开不同文件时，当前文件会先被关闭和清理；再次打开同一路径且文件仍处于打开状态时直接返回成功。

`GetSolverType()` 只读取第一个 `CGNSBase_t` 下直接声明的 `FlowEquationSet_t`，不遍历 Zone，也不根据 `SimulationType_t` 或其他节点推断方程类型。节点不存在或读取失败时，接口保留对应 CGNS 日志并返回 `"Unknown"`。

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
│   └── ReaderApiBase.h             # reader 接口、工厂及日志协议类型
├── src/
│   └── ExportFunctions.cpp              # Create/Destroy reader 导出
├── Common/
│   └── CgnsTypes.hpp               # 内部 CGNS 常量与网格拓扑数据类型
├── Core/
│   ├── CgnsCore.h                  # CGNS 层次遍历实现
│   ├── FileManager.h               # 文件生命周期、版本与 Base 级方程类型
│   ├── ReaderMeshData.h            # Base/Zone/Section 网格拓扑初始化
│   ├── ReaderFieldData.h           # 解场数据读取扩展点
│   └── src/
├── Utils/
│   ├── Logger.h                    # 实例 dispatcher、格式化与错误适配
│   └── src/Logger.cpp              # dispatcher 并发与生命周期实现
└── 3rdparty/
    └── cgns/                       # CGNS 静态库及其 CMake 配置
```

CGNS 层次、节点语义、元素类型和边界条件见 [`CGNS.md`](./CGNS.md)；仓库版本的完整 Mid-Level Library C API 见 [`CGNS_API.md`](./CGNS_API.md)。

## 构建与链接

该模块依赖根工程提供的 `3RD_ROOT` 和 vendored 依赖路径，应从仓库根目录配置：

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
