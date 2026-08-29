# Core

`Core` 是 StupidBhh 的命令行集成程序。它负责解析运行参数、初始化应用日志，并调用 `ReaderCGNS` 检查 CGNS 文件。

该目标更接近应用入口和集成验证程序，而不是通用基础库。可复用的 CGNS 检查能力由 `ReaderCGNS` 提供；跨项目工具则位于仓库根目录的 `Utils/` 与 `Logger/`。

## 职责边界

`Core` 当前承担以下职责：

- 管理命令行参数及工作目录；
- 初始化基于 spdlog 的异步日志系统；
- 通过通用的 `ModuleGuard` 管理 DLL 句柄，并由具体工具类解析所需导出；
- 在作用域内将 ReaderCGNS 日志转发到应用 logger；
- 通过 `ReaderAPI::ReaderCGNS` 实例输出 CGNS 文件结构信息；
- 提供内存映射文本读取、字符编码处理和进程调用等应用侧工具。

`Core` 不对外提供稳定的 C++ 库接口。需要集成 CGNS 检查能力时，应使用 `ReaderCGNS` 的公开头文件与 DLL 导出约定，而不是复用 `Core/src/Main.cpp` 或链接其生成的 import library。

## 运行流程

程序入口位于 `src/Main.cpp`，主要流程如下：

1. `SingletonData::ProcessArguments()` 解析并规范化参数；
2. 在工作目录下初始化控制台与文件日志；
3. 验证输入路径存在；
4. 构造 `AnalysisCGNS`，从可执行文件目录加载 `ReaderCGNS.dll`；
5. `AnalysisCGNS` 解析 `CreateReaderCGNS`/`DestroyReaderCGNS` 并创建 reader；
6. 通过 reader 实例注册静态日志回调，将 DLL 日志接入默认 spdlog logger；
7. 同步读取求解器类型和文件结构信息；
8. 关闭文件、清除回调、销毁 reader 并卸载 DLL。

## 命令行接口

| 参数 | 必需 | 说明 |
|---|---:|---|
| `--inputPath`, `-i` | 是 | 要检查的 CGNS 文件路径。 |
| `--workDirectory`, `-w` | 否 | 日志与生成文件目录。默认使用输入文件所在目录；仅传入文件名时使用 `./<文件名主干>/`。 |
| `--DEBUG` | 否 | 启用详细日志。Debug 构建会强制启用详细日志。 |
| `--help`, `-h` | 否 | 输出参数帮助。 |

示例：

```powershell
.\bin\Debug\Core.exe `
    --inputPath D:\data\case.cgns `
    --workDirectory D:\work\case `
    --DEBUG
```

运行时会在工作目录产生以下内容：

| 路径 | 内容 |
|---|---|
| `logs/stupid-bhh.log` | 应用及 ReaderCGNS 转发日志。 |

日志同时输出到控制台。CGNS 文件以只读方式打开，`ReaderAPI::ReaderCGNS::info()` 不修改输入文件。

## 目录结构

```text
Core/
├── CMakeLists.txt
├── src/
│   └── Main.cpp                    # 命令行入口与集成流程
├── Common/
│   ├── SingletonData.h             # 参数和应用级状态
│   ├── Functions.h                 # 仅依赖标准库的字符串工具
│   ├── Macros.hpp                  # 通用宏，当前包含作用域计时
│   ├── WindowsFunctions.h          # Win32 编码、进程、环境、路径和 DLL 句柄工具
│   └── src/
├── ReaderCGNS/
│   ├── AnalysisCGNS.h              # ReaderCGNS DLL 加载、实例与分析流程
│   └── src/
├── Utils/
│   ├── HighFiveUtils.hpp           # HDF5 数据集读写辅助函数
│   ├── MioReader.h                 # 内存映射文本读取器
│   └── src/
└── 3rdparty/
    └── hdf5/                       # Core 使用的 HDF5 运行库与 CMake 配置
```

## 依赖关系

| 依赖 | 用途 | 集成方式 |
|---|---|---|
| `ReaderCGNS` | CGNS 文件检查与日志回调 | 公开头 + 运行时 DLL；不链接 import library |
| Boost.Program_options | 命令行解析 | vendored CMake package |
| Boost.Container | 容器支持 | vendored CMake package |
| HDF5 | 数据文件后端 | 共享库 |
| HighFive | HDF5 C++ 封装 | 头文件库 |
| spdlog | 控制台与文件日志 | 头文件库 |
| mio | 内存映射文件读取 | 头文件库 |
| TBB | 标准并行算法后端 | 可选；未找到时并行算法退化为串行执行 |

依赖版本及仓库级工具链要求以根目录 [`README.md`](../README.md) 为准。

## 构建

`Core/CMakeLists.txt` 依赖根工程定义的公共路径，因此应从仓库根目录配置。完整运行布局需要同时构建默认 all target，使 `Core.exe` 与 `ReaderCGNS.dll` 写入同一配置目录：

```powershell
cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64
cmake --build build/Debug --config Debug
cmake --build build/Debug --config Release
```

输出位于：

```text
bin/Debug/Core.exe
bin/Debug/ReaderCGNS.dll
bin/Release/Core.exe
bin/Release/ReaderCGNS.dll
```

CMake 的构建后步骤会将 `Core` 链接依赖的 DLL 以及 HDF5 的压缩运行库复制到可执行文件目录。`ReaderCGNS.dll` 不是链接依赖，它依靠根工程的统一输出目录与 `Core.exe` 放在一起；仅执行 `--target Core` 不会构建该 DLL。

`LoadModuleGuard()` 是通用的 DLL 加载入口：它接收 `std::filesystem::path`，使用 `LoadLibraryW` 加载模块，并由 `ModuleGuard` 在析构时调用 `FreeLibrary`。该工具不解析任何具体导出；`ReaderCGNS.dll` 的导出解析和 reader 生命周期由 `AnalysisCGNS` 负责。

## 日志生命周期

`AnalysisCGNS` 在 reader 创建后调用实例级 `SetLogCallback()`，静态适配回调通过 `spdlog::default_logger()` 转发日志。日志注册失败是非致命状态：Core 使用自身 logger 记录警告，reader 仍可继续检查文件。

该适配依赖以下应用不变量：

- 构造 `AnalysisCGNS` 前，`ProcessArguments()` 已完成默认 logger 初始化；
- `AnalysisCGNS` 存活期间，不替换或关闭默认 logger；
- 销毁 `AnalysisCGNS` 前，所有针对其 reader 的 API 调用都已结束。

Debug 构建中，日志回调将 ReaderCGNS 提供的源码路径和行号作为 spdlog 的 `source_loc`，因此日志格式中的源码位置指向 DLL 内部调用点，而不是 Core 的回调函数。ReaderCGNS 不预先截取文件名；当前 spdlog `%s` 格式负责显示短文件名。Release 构建的日志格式不输出源码位置。

`AnalysisCGNS` 析构时先关闭文件以保留关闭日志，再调用实例级 `ClearLogCallback()` 等待已经进入的回调结束，随后销毁 reader，最后由 `ModuleGuard` 卸载 DLL。显式销毁 reader 和成员声明顺序共同固定该生命周期。

## 开发约定

- 应用入口保持轻量，通用能力优先下沉到明确归属的模块；
- `Core/Utils` 只放置应用侧适配器，不应扩展 ReaderCGNS 的公开协议；
- 修改参数、输出文件或构建依赖时，同步更新本文档和根 `README.md`；
- 第三方内容位于 `Core/3rdparty/`，除有计划的依赖升级外不要直接修改。
