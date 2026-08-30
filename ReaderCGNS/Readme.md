# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 动态加载模块。它基于官方 CGNS Mid-Level Library 遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出。

当前公开能力定位为“结构检查与诊断”，而不是完整的数据导入器：接口可读取文件版本和 Base 级方程类型，`ReaderAPI::ReaderApiBase::info()` 输出元数据和连接摘要，但不返回可供业务计算使用的网格对象。

## 能力范围

当前检查流程覆盖：

- CGNS 存储类型、文件版本与数据精度；
- Base、Zone、迭代信息与结构化/非结构化尺寸；
- FlowSolution、DiscreteData 与 ZoneSubRegion；
- GridCoordinates 与元素 Section/Connectivity；
- 一对一连接、一般网格连接与 Overset Holes；
- BoundaryCondition 及其 DataSet；
- 刚体和任意网格运动；
- ParticleZone、粒子坐标与粒子解描述。

输入文件使用 `CG_MODE_READ` 打开。库不会修改 CGNS 文件，也不会主动创建日志文件；所有可观测信息均交给调用方注册的回调。

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
| `ReaderAPI::ReaderApiBase::info()` | 遍历当前已打开文件并输出结构检查信息。 |

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

`Open()` 会验证 CGNS 文件类型并以 `CG_MODE_READ` 打开文件；文件无效或 `cg_open()` 失败时返回 `false`。调用方应仅在 `Open()` 成功且 `IsOpen()` 为 `true` 时调用 `GetVersion()`、`GetSolverType()` 和 `info()`，并在结束后显式调用 `Close()`。当前实现不支持在未关闭旧文件时复用同一实例打开另一个文件。

`GetSolverType()` 只读取第一个 `CGNSBase_t` 下直接声明的 `FlowEquationSet_t`，不遍历 Zone，也不根据 `SimulationType_t` 或其他节点推断方程类型。节点不存在或读取失败时，接口保留对应 CGNS 日志并返回 `"Unknown"`。

`info()` 没有返回值。节点级 CGNS API 错误不会汇总为调用结果，而是记录对应状态与 `cg_get_error()` 后在可行时继续。因此日志内容是判断局部读取问题的主要依据。`Close()` 同样没有返回值，关闭失败通过日志报告。

内部 `CG_INFO(expression)` 适配器只求值一次 CGNS API 表达式。状态为 `CG_OK` 时不输出；其他状态会连同状态名称或数值、调用表达式、`cg_get_error()` 文本及调用位置交给日志回调，并原样返回状态码。它不会抛出异常、提前返回或改变调用方控制流；允许调用点忽略返回值，需要根据失败结果分支时也可以显式检查该返回值。

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
│   └── ReaderCGNS.cpp              # Create/Destroy reader 导出
├── Common/
│   └── CgnsTypes.hpp               # 内部 CGNS 公共常量
├── Core/
│   ├── CgnsCore.h                  # CGNS 层次遍历实现
│   ├── FileManager.h               # 文件生命周期、版本与 Base 级方程类型
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
