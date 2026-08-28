# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 共享库。它基于官方 CGNS Mid-Level Library 遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出。

当前公开能力定位为“结构检查与诊断”，而不是完整的数据导入器：`ReaderCGNSBase::info()` 输出元数据和连接摘要，但不返回可供业务计算使用的网格对象。

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

公开头文件位于 `include/ReaderCGNS/`：

```cpp
#include "ReaderCGNS/ReaderCGNS.h"
```

| API | 作用 |
|---|---|
| `ReaderCGNSBase::Open(path)` | 以只读方式打开 CGNS 文件。 |
| `ReaderCGNSBase::Close()` | 关闭当前文件。 |
| `ReaderCGNSBase::IsOpen()` | 查询文件是否已打开。 |
| `ReaderCGNSBase::info()` | 输出当前文件的结构检查信息。 |
| `ReaderCGNSBase::QueryInterface()` | 查询实现提供的扩展接口。 |

日志级别依次为 `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR` 和 `CRITICAL`。

## 动态加载约定

ReaderCGNS 的交付物是 `include/ReaderCGNS/` 下的公开头和 `ReaderCGNS.dll`，调用方不依赖 import library。DLL 提供以下稳定名称，由调用方通过 `GetProcAddress` 解析：

- reader 生命周期：`CreateReaderCGNS`、`DestroyReaderCGNS`；
- 日志控制：`SetLogCallback`、`ClearLogCallback`、`SetMinimumLogLevel`、`GetMinimumLogLevel`。

公开头只提供 `ReaderCGNSBase`、reader 工厂函数指针类型以及日志级别和回调类型，不声明需要 import library 的 C++ Logger 函数。完整的动态加载流程见 `Core/src/Main.cpp`，日志回调的 RAII 封装见 `Core/Utils/ReaderCGNSLogGuard`。

## 日志并发约定

ReaderCGNS 的日志注册表是进程级共享状态，同一时刻只维护一个回调和一个上下文指针：

- 回调在触发日志的 ReaderCGNS 线程上同步执行；
- 多个 ReaderCGNS 调用可能并发进入同一个回调，调用方必须保证回调线程安全；
- `context`、`file` 和 `message` 都是借用数据，仅在本次回调期间有效；
- 更换或清除回调时，库会等待其他线程中已经开始的回调结束；
- 不允许在回调内部调用 `SetLogCallback()` 或 `ClearLogCallback()`，此类调用会返回 `false`；
- 回调抛出的异常会被库捕获，异常不会越过动态库边界传播。

上下文对象的生命周期由调用方负责，并且必须长于回调注册期。日志注册表的并发保护不代表底层 CGNS/HDF5 构建支持任意并发文件访问；并发读取策略仍应遵循所使用 CGNS 与 HDF5 库的线程安全配置。

## 返回值与错误处理

`ReaderCGNSBase::info()` 在以下关键阶段失败时返回 `false`：

- 输入无法识别为有效 CGNS 文件；
- 文件无法以只读模式打开；
- 遍历结束后文件关闭失败。

节点级 CGNS API 错误会记录对应状态与 `cg_get_error()` 信息；检查流程会在可行时继续遍历后续节点。因此调用方应同时检查返回值和日志内容。

未安装日志回调时，检查仍可执行，但不会向应用输出结构信息。

## 目录结构

```text
ReaderCGNS/
├── CMakeLists.txt
├── Readme.md
├── CGNS.md                         # CGNS 数据结构与 C API 参考
├── include/ReaderCGNS/
│   ├── ReaderCGNS.h                # 导出 API
│   └── ReaderCGNSTypes.hpp         # 日志级别与回调类型
├── src/
│   └── ReaderCGNS.cpp              # 文件检查与层次遍历
├── Core/
│   ├── CgnsCore.h                  # 内部文件句柄封装
│   ├── CgnsTypes.hpp
│   └── src/
├── Utils/
│   ├── Logger.h                    # 内部格式化与错误适配
│   └── src/Logger.cpp              # 并发安全的回调注册表
└── 3rdparty/
    └── cgns/                       # CGNS 静态库及其 CMake 配置
```

CGNS 层次、元素类型、边界条件及常用 Mid-Level Library API 的详细说明见 [`CGNS.md`](./CGNS.md)。

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

仓库内其他 CMake 目标可直接链接：

```cmake
target_link_libraries(YourTarget PRIVATE ReaderCGNS)
```

`ReaderCGNS` 以共享库形式构建，CGNS 依赖以 `CGNS::cgns_static` 私有链接。公开 include 目录通过目标使用要求自动传递；调用方不应直接包含 `Core/` 或 `Utils/` 下的内部头文件。

## 开发约定

- 对外兼容面仅包含 `include/ReaderCGNS/` 下的头文件和导出符号；
- 新增公开 API 时，同时说明所有权、线程安全、错误与生命周期语义；
- 保持 CGNS 文件只读，除非通过独立设计明确引入写入接口；
- 新增遍历节点时，使用统一日志层级并保留 CGNS 错误上下文；
- 不在共享库内部绑定 spdlog 等应用日志框架，日志后端由调用方决定；
- 第三方内容位于 `3rdparty/cgns/`，只在有计划的依赖升级中修改。
