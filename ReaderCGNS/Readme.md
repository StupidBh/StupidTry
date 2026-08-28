# ReaderCGNS

`ReaderCGNS` 是一个以只读方式检查 CGNS 文件的 C++ 共享库。它基于官方 CGNS Mid-Level Library 遍历文件层次，将文件类型、网格结构、解数据描述、连接关系和边界条件等信息通过调用方提供的日志回调输出。

当前公开能力定位为“结构检查与诊断”，而不是完整的数据导入器：`ReaderAPI::ReaderCGNS::info()` 输出元数据和连接摘要，但不返回可供业务计算使用的网格对象。

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
#include "ReaderCGNS/ReaderCGNSTypes.hpp" // 使用日志接口时需要
```

| API | 作用 |
|---|---|
| `ReaderAPI::ReaderCGNS::Open(path)` | 以只读方式打开 CGNS 文件。 |
| `ReaderAPI::ReaderCGNS::Close()` | 关闭当前文件。 |
| `ReaderAPI::ReaderCGNS::IsOpen()` | 查询文件是否已打开。 |
| `ReaderAPI::ReaderCGNS::info()` | 遍历当前已打开文件并输出结构检查信息。 |
| `ReaderAPI::ReaderCGNS::QueryInterface()` | 返回实现扩展入口；当前没有公开的扩展类型协议。 |

日志级别依次为 `TRACE`、`DEBUG`、`INFO`、`WARN`、`ERROR` 和 `CRITICAL`。

## 动态加载约定

ReaderCGNS 的交付物是 `include/ReaderCGNS/` 下的公开头和 `ReaderCGNS.dll`，调用方不依赖 import library。DLL 提供以下稳定名称，由调用方通过 `GetProcAddress` 解析：

- reader 生命周期：`CreateReaderCGNS`、`DestroyReaderCGNS`；
- 日志控制：`SetLogCallback`、`ClearLogCallback`、`SetMinimumLogLevel`、`GetMinimumLogLevel`。

`ReaderCGNS.h` 提供 `ReaderAPI::ReaderCGNS` 和 reader 工厂函数指针类型，`ReaderCGNSTypes.hpp` 提供日志级别与回调类型。公开头不声明需要 import library 的日志控制函数；调用方应按上述稳定名称声明本地函数指针并动态解析。完整流程见 `Core/src/Main.cpp`，日志回调的 RAII 封装见 `Core/Utils/ReaderCGNSLogGuard`。

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

`Open()` 会验证 CGNS 文件类型并以 `CG_MODE_READ` 打开文件；文件无效或 `cg_open()` 失败时返回 `false`。调用方应仅在 `Open()` 成功且 `IsOpen()` 为 `true` 时调用 `info()`，并在结束后显式调用 `Close()`。当前实现不支持在未关闭旧文件时复用同一实例打开另一个文件。

`info()` 当前完成遍历后返回 `true`，节点级 CGNS API 错误不会汇总到返回值，而是记录对应状态与 `cg_get_error()` 后在可行时继续。因此日志内容是判断局部读取问题的主要依据。`Close()` 没有返回值，关闭失败同样通过日志报告。

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
│   └── ReaderCGNS.cpp              # Create/Destroy reader 导出
├── Core/
│   ├── CgnsCore.h                  # CGNS 层次遍历实现
│   ├── CgnsTypes.hpp
│   ├── FileManager.h               # 文件打开、关闭与句柄状态
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

同一源码树中的调用目标应显式使用公开 include 路径；如需保证构建顺序，可添加 target 依赖，但不要链接 ReaderCGNS import library：

```cmake
target_include_directories(YourTarget PRIVATE
        ${CMAKE_SOURCE_DIR}/ReaderCGNS/include
)
add_dependencies(YourTarget ReaderCGNS)
```

`ReaderCGNS` 以共享库形式构建，CGNS 依赖以 `CGNS::cgns_static` 私有链接。Visual Studio 构建可能同时生成 `ReaderCGNS.lib`，但该 import library 不属于公开交付约定。调用方只应分发公开头和 DLL，并且不应直接包含 `Core/` 或 `Utils/` 下的内部头文件。

## 开发约定

- 对外兼容面仅包含 `include/ReaderCGNS/` 下的头文件和导出符号；
- 新增公开 API 时，同时说明所有权、线程安全、错误与生命周期语义；
- 保持 CGNS 文件只读，除非通过独立设计明确引入写入接口；
- 新增遍历节点时，使用统一日志层级并保留 CGNS 错误上下文；
- 不在共享库内部绑定 spdlog 等应用日志框架，日志后端由调用方决定；
- 第三方内容位于 `3rdparty/cgns/`，只在有计划的依赖升级中修改。
