# Core

`Core` 是 StupidBhh 的命令行集成程序。它负责解析运行参数、初始化应用日志，并调用 `ReaderCGNS` 检查 CGNS 文件。

该目标更接近应用入口和集成验证程序，而不是通用基础库。可复用的 CGNS 检查能力由 `ReaderCGNS` 提供，公开协议位于仓库根目录的 `ReaderAPI/`；通用头文件工具位于根目录的 `Utils/`，应用日志封装位于 `Core/Utils/`。

## 职责边界

`Core` 当前承担以下职责：

- 管理命令行参数及工作目录；
- 初始化基于 spdlog 的异步日志系统；
- 通过通用的 `ModuleGuard` 管理 DLL 句柄，并由具体工具类解析所需导出；
- 在作用域内将 ReaderCGNS 日志转发到应用 logger；
- 通过 `ReaderAPI::ReaderApiBase` 实例读取求解器类型、节点坐标、单元、组件名称及其成员 ID，并批量读取场值、输出字段摘要；
- 检查返回的节点和单元 ID 是否重复，并对单元连接数据做范围诊断；
- 提供内存映射文本读取和进程调用等应用侧工具。

`Core` 不对外提供稳定的 C++ 库接口。需要集成 CGNS 检查能力时，应使用 `ReaderCGNS` 的公开头文件与 DLL 导出约定，由调用方管理 reader 实例。

## 运行流程

程序入口位于 `src/Main.cpp`，主要流程如下：

1. `SingletonData::ProcessArguments()` 解析并规范化参数；
2. 按日志级别初始化控制台日志，并在日志目录下初始化文件日志；
3. 验证输入路径存在；
4. 构造 `AnalysisCGNS`，从可执行文件目录加载 `ReaderCGNS.dll`；
5. `AnalysisCGNS` 解析 `CreateReaderCGNS`/`DestroyReaderCGNS` 并创建 reader；
6. 通过 reader 实例注册静态日志回调，将 DLL 日志接入默认 spdlog logger；
7. 打开文件，依次查询求解器类型、全部节点和全部单元，检查重复 ID 与连接数据范围，再查询组件名称及成员 ID、场函数名称并批量读取场值；
8. 关闭文件、清除回调、销毁 reader 并卸载 DLL。

当前 `AnalysisCGNS::Analyze()` 中的 `info()` 调用已注释，默认不输出完整 CGNS 节点树，也没有启用它的命令行选项。节点和单元查询成功时分别由 ReaderCGNS 记录 `All nodes`、`All elements` 数量；组件查询会读取每个组件的成员 ID 并检查其是否落在扁平单元数组范围内。

节点与单元分别检查 ID 唯一性，重复时记录 `Repeat node` 或 `Repeat elem` 警告。单元 ID 按扁平输出顺序从 0 开始，并与输出数组下标一致；组件成员 ID 复用这一编号，Core 会检查其范围。当前连接范围诊断将 `Elem::nodes` 中的每个值与 `[0, 节点数量)` 比较；它尚未解析 NFACE 的面节点数前缀，因此可能把长度当作节点 ID 而误报。该诊断不等于完整拓扑验证，数据布局见 [多面体展开说明](../ReaderCGNS/Readme.md#多面体展开)。

字段批量读取至少成功一项时输出 `Field Function sum` 和各成功字段的名称、`type`、`ids` 数量及 `value` 数量，不逐项打印数值。其中 `sum` 是枚举到的字段名称数量，部分读取失败时不等于成功字段数。场值支持范围和编号规则见 [ReaderCGNS 文档](../ReaderCGNS/Readme.md#场值读取)。

## 命令行接口

| 参数 | 必需 | 说明 |
|---|---:|---|
| `--input_file` | 是 | 要检查的 CGNS 文件路径。 |
| `--workspace_dir` | 否 | 工作目录，不改变进程当前目录。默认使用输入文件所在目录；仅传入文件名时使用 `./<文件名主干>/`。 |
| `--log_dir` | 否 | 日志文件目录，默认使用 `<workspace_dir>/logs/`。 |
| `--log_level` | 否 | 接受 `trace`、`debug`、`info`、`warning`、`error`，默认为 `info`；Debug 构建会强制使用 `trace`。 |
| `--help` | 否 | 输出参数帮助。 |

旧参数 `--inputPath`、`--workDirectory`、`--DEBUG` 及短选项 `-i`、`-w`、`-h` 已移除，请使用上表中的参数。

示例：

```powershell
.\bin\Debug\Core.exe `
    --input_file D:\data\case.cgns `
    --workspace_dir D:\work\case `
    --log_dir D:\work\case\logs `
    --log_level debug
```

运行时会在日志目录（默认 `<workspace_dir>/logs/`）产生以下内容：

| 路径 | 内容 |
|---|---|
| `stupid-bhh_YYYY-MM-DD.log` | 应用及 ReaderCGNS 转发日志，按日期命名，每天午夜轮换，最多保留 30 个日志文件。 |

日志同时输出到控制台；文件日志初始化失败时会向标准错误报告并继续使用控制台日志。CGNS 文件以只读方式打开，当前流程不生成网格或场值导出文件。

### 退出状态

参数解析失败、`--help`、DLL/reader 初始化失败或文件打开失败返回 `EXIT_FAILURE`；输入路径不存在时返回 `-1`。打开文件后的节点、单元、组件或字段查询失败，以及重复 ID、组件成员范围和连接范围警告，都不会使 `Analyze()` 返回 `false`，且 `main()` 捕获分析异常后仍会返回 `0`。因此当前退出码不能作为数据完整性或全部查询成功的判据，应检查日志与字段摘要。

## 目录结构

```text
Core/
├── CMakeLists.txt
├── src/
│   └── Main.cpp                    # 命令行入口与集成流程
├── Common/
│   ├── SingletonData.h             # 参数和应用级状态
│   ├── Functions.h                 # 字符串、环境变量、进程调用和可执行文件路径工具
│   ├── Macros.hpp                  # 通用宏，当前包含作用域计时
│   ├── WindowsFunctions.h          # Win32 DLL 句柄工具
│   └── src/
├── ReaderCGNS/
│   ├── AnalysisCGNS.h              # ReaderCGNS DLL 加载、实例与分析流程
│   └── src/
├── Utils/
│   ├── HighFiveUtils.hpp           # HDF5 数据集读写辅助函数
│   ├── MioReader.h                 # 内存映射文本读取器
│   ├── logger.hpp                 # 基于 spdlog 的异步日志封装
│   ├── logger_formatter.hpp       # 日志格式化辅助工具
│   └── src/
├── tests/                         # CTest 回归测试
│   ├── ReaderLogLifetimeTests.cpp  # 日志路径与 DLL 卸载后的队列生命周期
│   └── MioReaderLineTests.cpp      # 换行、批量状态及长行读取
└── 3rdparty/                      # Core 使用的第三方依赖
    ├── boost/                     # Boost 库与 CMake 配置
    ├── hdf5/                      # HDF5 运行库与 CMake 配置
    ├── highfive/                  # HDF5 C++ 头文件封装
    ├── meojson/                   # JSON/JSON5 头文件库
    ├── mio/                       # 内存映射文件头文件库
    └── spdlog/                    # 日志头文件库
```

`Functions.h` 中的 `GetEnv()` 使用 `std::getenv()` 读取环境变量；变量不存在时抛出 `std::runtime_error`。`GetExecutablePath()` 和 `GetExecutableDirectory()` 使用 Boost.Process 查询当前进程的可执行文件路径，供 `AnalysisCGNS` 定位同目录下的 `ReaderCGNS.dll`。`ExecuteProcess()` 使用 Boost.Process 启动子进程并转发其输出，支持通过回调控制进程收尾行为。

## 文本读取

`MioReader` 使用内存映射读取在 reader 使用期间保持不变的文本文件，`GetLine()` 与 `GetLineBatch()` 共享向前推进的读取位置。两者支持文件内混合使用 LF、CR、CRLF；只有相邻的 CRLF 合并为一个分隔符，连续分隔符产生空行，末行可以没有换行，末尾分隔符不会额外产生一行。读取器不做字符编码转换或删除 BOM，数值解析仍由原有的 `parse_line()` 完成。

批量读取缓存下一个 LF 的偏移，包括“剩余内容没有 LF”的结果，并只在该 LF 之前搜索 CR。缓存跨批次保留；逐行读取越过缓存位置后，下次批量读取会重新搜索。它只保存一个位置，不建立整文件行索引，也不使用固定长度窗口，因此可处理很长的行，并避免纯 LF、纯 CR 文件反复扫描整个剩余文件。读取期间修改文件内容会使缓存失效，不属于该读取器的使用场景。

返回的 `std::string_view` 直接引用映射区域，不复制行正文；其有效期取决于底层映射的生命周期。输出视图数组的容量由 `max_lines` 决定，默认每批最多 10000 行。映射仍覆盖整个文件的虚拟地址空间，驻留页由操作系统管理；固定大小的换行缓存不代表进程总内存恒定，调用方解析出的数值数组也会占用内存。

## 依赖关系

| 依赖 | 用途 | 集成方式 |
|---|---|---|
| `ReaderAPI/` + `ReaderCGNS` | CGNS 文件检查与日志回调 | 根目录公开头 + 运行时 DLL；不链接 import library |
| Boost.Process | 子进程执行与当前进程可执行文件路径查询 | vendored CMake package |
| Boost.Program_options | 命令行解析 | vendored CMake package |
| HDF5 | 数据文件后端 | 共享库 |
| HighFive | HDF5 C++ 封装 | 头文件库 |
| spdlog | 控制台与文件日志 | 头文件库 |
| mio | 内存映射文件读取 | 头文件库 |
| TBB | 部分标准库实现的并行算法后端 | 可选；未找到时不链接 `TBB::tbb`，MSVC STL 不依赖它 |

必需第三方依赖均位于 `Core/3rdparty/`。CMake 将该目录和仓库根目录加入 Core 的私有 include 路径，并分别设置模块内的 `BOOST_ROOT`、`HDF5_ROOT` 查找包；公开协议通过 `#include "ReaderAPI/ReaderApiBase.h"` 引用。依赖版本及仓库级工具链要求以根目录 [`README.md`](../README.md) 为准。

## 构建

`Core/CMakeLists.txt` 使用仓库根目录下的公开协议和通用工具，并依赖根工程统一设置编译选项、MSVC 运行库及输出目录，因此应从仓库根目录配置。完整运行布局需要同时构建默认 all target，使 `Core.exe` 与 `ReaderCGNS.dll` 写入同一配置目录：

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

默认开启 `BUILD_TESTING`，同时构建 `CoreReaderLogLifetimeTests` 和 `CoreMioReaderLineTests`。回归测试位于 `Core/tests/`，通过 CTest 运行：

```powershell
ctest --test-dir build/Debug -C Debug --output-on-failure
ctest --test-dir build/Debug -C Release --output-on-failure
```

`Core.ReaderLogLifetime` 检查已有全局线程池配置的保留、路径文本所有权、扩容后的地址稳定性和并发查找；它还暂停异步日志线程，加载并卸载实际的 `ReaderCGNS.dll`，再验证排队日志。Debug 检查 DLL 源码位置仍可格式化，Release 检查回调不附带源码位置。测试使用仓库内的非 CGNS 文本文件触发错误日志，不需要 CGNS 样例；配置时传入 `-DBUILD_TESTING=OFF` 可关闭测试目标。

`Core.MioReaderLines` 使用独立的分行参考解析器检查 9841 种文本/换行排列，并覆盖六种批量大小、逐行与批量交替读取、默认批量大小、UTF-8/BOM/NUL、EOF 行为和移动后的读取状态。长行用例包括 4 MiB 的单行，以及由 256 条 64 KiB 长行组成的混合换行文件。测试输入在临时目录生成，读取期间不修改，结束后清理。

`LoadModuleGuard()` 是通用的 DLL 加载入口：它接收 `std::filesystem::path`，使用 `LoadLibraryW` 加载模块，并由 `ModuleGuard` 在析构时调用 `FreeLibrary`。该工具不解析任何具体导出；`ReaderCGNS.dll` 的导出解析和 reader 生命周期由 `AnalysisCGNS` 负责。

## 日志生命周期

`AnalysisCGNS` 在 reader 创建后调用实例级 `SetLogCallback()`，静态适配回调通过 `spdlog::default_logger()` 转发日志。日志注册失败是非致命状态：Core 使用自身 logger 记录警告，reader 仍可继续检查文件。

Logger 首次构造时只在 `spdlog::thread_pool()` 为空的情况下创建默认线程池；已由调用方配置的全局线程池会被保留。这样首次保存源码路径不会覆盖现有异步 logger 使用的线程池。

该适配依赖以下应用不变量：

- 构造 `AnalysisCGNS` 前，`ProcessArguments()` 已完成默认 logger 初始化；
- `AnalysisCGNS` 存活期间，不替换或关闭默认 logger；
- 销毁 `AnalysisCGNS` 前，所有针对其 reader 的 API 调用都已结束。

Debug 构建中，日志回调先通过 `dylog::Logger::InternSourceFile()` 将 ReaderCGNS 提供的完整源码路径复制到 Core 的路径表，再将表内的稳定指针和原行号作为 spdlog 的 `source_loc`。路径按文本内容去重，查找与插入由独立互斥锁保护；已有路径不会因新增路径或 rehash 改变地址。因此日志格式中的源码位置仍指向 DLL 内部调用点，而不是 Core 的回调函数。ReaderCGNS 不预先截取文件名；当前 spdlog `%s` 格式负责显示短文件名。Release 构建不保存回调路径，日志格式也不输出源码位置。

`AnalysisCGNS` 析构时先关闭文件以保留关闭日志，再调用实例级 `ClearLogCallback()` 等待已经进入的回调结束，随后销毁 reader，最后由 `ModuleGuard` 卸载 DLL。显式销毁 reader 和成员声明顺序共同固定该生命周期。

路径表由应用 Logger 持有，不随 reader 销毁、DLL 卸载或默认 logger 替换而清空。Logger 析构函数先调用 `spdlog::shutdown()`，再由成员析构释放路径字符串。当前 Core 不长期持有线程池的外部强引用，shutdown 会释放全局池的最后一个持有者，排空队列并等待工作线程结束，保证源码指针在格式化期间有效。若调用方还持有全局线程池的 `shared_ptr`，或通过 `UpdateLog()` 接入独立的外部异步线程池，shutdown 本身不保证这些线程已经结束；调用方必须在应用 Logger 销毁前排空并结束所有仍会访问路径表的日志线程。`ClearLogCallback()` 只等待 DLL 回调结束，不等待 Core 的异步队列消费；它不负责路径表释放。

## 开发约定

- 应用入口保持轻量，通用能力优先下沉到明确归属的模块；
- `Core/Utils/` 放置应用侧工具与日志封装，跨模块通用头文件放入根目录 `Utils/`，公开协议放入根目录 `ReaderAPI/`；
- 修改参数、输出文件或构建依赖时，同步更新本文档和根 `README.md`；
- 第三方内容位于 `Core/3rdparty/`，除有计划的依赖升级外不要直接修改。
