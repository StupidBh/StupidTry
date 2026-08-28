# Core

`Core` 是 StupidBhh 的命令行集成程序。它负责解析运行参数、初始化应用日志、调用 `ReaderCGNS` 检查 CGNS 文件，并执行当前的 HighFive/HDF5 数据写入与回读流程。

该目标更接近应用入口和集成验证程序，而不是通用基础库。可复用的 CGNS 检查能力由 `ReaderCGNS` 提供；跨项目工具则位于仓库根目录的 `Utils/` 与 `Logger/`。

## 职责边界

`Core` 当前承担以下职责：

- 管理命令行参数及工作目录；
- 初始化基于 spdlog 的异步日志系统；
- 通过 `LoadLibraryW`/`GetProcAddress` 加载 `ReaderCGNS.dll` 的稳定导出；
- 在作用域内将 ReaderCGNS 日志转发到应用 logger；
- 通过 `ReaderAPI::ReaderCGNS` 实例输出 CGNS 文件结构信息；
- 演示 HighFive 对普通数组、复合类型和可扩展数据集的写入与读取；
- 提供内存映射文本读取、字符编码处理和进程调用等应用侧工具。

`Core` 不对外提供稳定的 C++ 库接口。需要集成 CGNS 检查能力时，应使用 `ReaderCGNS` 的公开头文件与 DLL 导出约定，而不是复用 `Core/src/Main.cpp` 或链接其生成的 import library。

## 运行流程

程序入口位于 `src/Main.cpp`，主要流程如下：

1. `SingletonData::ProcessArguments()` 解析并规范化参数；
2. 在工作目录下初始化控制台与文件日志；
3. 验证输入路径存在；
4. 从可执行文件搜索路径加载 `ReaderCGNS.dll`；
5. 构造 `ReaderCGNSLogGuard`，由 guard 解析日志导出并接入默认 spdlog logger；
6. 解析 `CreateReaderCGNS`/`DestroyReaderCGNS`，同步执行 `Open()`、`info()` 和 `Close()`；
7. 销毁 reader、清除日志回调并卸载 DLL；
8. 在工作目录创建 `Try1.h5`，并行写入普通、复合及可扩展数据集；
9. 回读复合数据集。

当前 HighFive 逻辑是集成与性能探索代码。对同一个 HDF5 文件的并发访问能力取决于所使用的 HDF5 构建配置，不应将该示例直接视为通用的并发写入保证。

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
| `Try1.h5` | HighFive/HDF5 示例输出，现有流程会覆盖同名文件。 |

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
│   ├── WindowsFunctions.h          # Win32 编码、进程、环境和路径工具
│   └── src/
├── ReaderCGNS/
│   ├── ReaderCGNSLogGuard.h        # ReaderCGNS 日志的 RAII 适配器
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

## 日志生命周期

`ReaderCGNSLogGuard` 依赖以下应用不变量：

- 构造 guard 前，`ProcessArguments()` 已完成默认 logger 初始化；
- guard 存活期间，不替换或关闭默认 logger；
- 所有可能触发 ReaderCGNS 日志的线程在 guard 析构前结束。

析构函数会调用从 DLL 解析出的 `ClearLogCallback`，并等待其他线程中正在执行的回调退出。guard 必须先于 DLL 模块句柄析构；新增异步任务时，也必须确保所有 ReaderCGNS 调用在 guard 和模块卸载前结束。

## 开发约定

- 应用入口保持轻量，通用能力优先下沉到明确归属的模块；
- `Core/Utils` 只放置应用侧适配器，不应扩展 ReaderCGNS 的公开协议；
- 修改参数、输出文件或构建依赖时，同步更新本文档和根 `README.md`；
- 第三方内容位于 `Core/3rdparty/`，除有计划的依赖升级外不要直接修改。
