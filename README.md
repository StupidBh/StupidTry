# StupidBhh

Windows x64 上的 CGNS 只读检查与数据查询项目：`Core.exe` 提供命令行入口，运行时加载 `ReaderCGNS.dll`，读取求解器类型、网格单元、集合名称以及支持的节点/单元中心场值。完整结构诊断由 DLL 的 `info()` 接口提供，当前命令行流程不调用它。

## 工具链与语言标准

- **C++23**（`Core` 和 `ReaderCGNS` 通过 `target_compile_features(... cxx_std_23)` 要求至少 C++23）
- **CMake 4.3+**
- **首选生成器：Visual Studio 18 2026（x64）**
- **MSVC 使用 `/EHsc` 启用标准 C++ 异常展开语义**
- **MSVC 目标统一使用动态运行库：Debug 为 `/MDd`，其他配置为 `/MD`**
- **当前第一方目标使用 Win32 API，支持平台为 Windows x64**

## 构建与测试

优先使用 Visual Studio 18 安装目录中附带的 CMake、MSBuild 和 LLVM 工具；如果系统 `PATH` 中的 CMake 不支持 VS 18 生成器，应先定位 VS 18 自带的 CMake。

```powershell
cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64
cmake --build build/Debug --config Debug
ctest --test-dir build/Debug -C Debug
cmake --build build/Debug --config Release
ctest --test-dir build/Debug -C Release
```

默认构建会同时生成 `Core.exe` 和 `ReaderCGNS.dll`。`Core` 不链接 ReaderCGNS import library，而是在运行时从可执行文件目录加载 DLL；因此只构建 `Core` target 不会得到完整的可运行布局。

`main` 已集成三个第一方 CTest 测试：`Utils.GenericUtilities`、`Core.Functions` 和 `ReaderCGNS.LoggerCallback`，分别覆盖通用工具、Core 字符串/Win32 工具和 reader 日志回调。`BUILD_TESTING` 默认开启；只构建生产目标时可在配置命令中传入 `-DBUILD_TESTING=OFF`。

## 运行与验证

在仓库根目录运行，替换为实际 CGNS 文件和日志目录；Release 构建将路径中的 `Debug` 改为 `Release`：

```powershell
.\bin\Debug\Core.exe --inputPath D:\data\case.cgns --workDirectory D:\work\case
```

日志输出到控制台和工作目录的 `logs/stupid-bhh_YYYY-MM-DD.log`，包括单元数量、集合名称及成功读取字段的类型、ID 数量和值数量。场值读取目前仅支持每个 Zone 至多一个 FlowSolution，位置为 `Vertex` 或 `CellCenter`，详见 [ReaderCGNS 的场值读取说明](ReaderCGNS/Readme.md#场值读取)。

当前测试未覆盖真实 CGNS 网格和场值读取，也没有随仓库提供 CGNS 测试样例。构建后可用 `Core.exe --help` 检查程序能否启动，再用实际文件验证 DLL 加载和读取；当前帮助命令返回非零退出码，读取流程的零退出码也不代表全部查询成功，需结合日志检查。

## 工程目录

```text
StupidTry/
├── CMakeLists.txt                  # 根 CMake 入口，配置 MSVC 选项、输出目录和子工程
├── Core/                           # 主程序 Core 可执行文件
│   ├── CMakeLists.txt
│   ├── Readme.md                    # Core 架构、运行与开发说明
│   ├── src/
│   │   └── Main.cpp                # 命令行入口与 CGNS 分析流程
│   ├── Common/                     # 参数处理、全局配置等通用实现
│   │   ├── Functions.h             # 仅依赖标准库的字符串工具
│   │   ├── SingletonData.h
│   │   ├── WindowsFunctions.h      # Win32 编码、进程、环境和路径工具
│   │   └── src/
│   ├── ReaderCGNS/                 # ReaderCGNS 的应用侧集成
│   │   ├── AnalysisCGNS.h          # DLL 加载、reader 生命周期和日志适配
│   │   └── src/
│   ├── Utils/                      # Core 专用的 HDF5 和文件 I/O 工具
│   │   ├── HighFiveUtils.hpp
│   │   ├── MioReader.h
│   │   └── src/
│   └── 3rdparty/
│       └── hdf5/                  # Core 使用的 HDF5 依赖
├── ReaderCGNS/                     # CGNS 文件读取与检查共享库
│   ├── CMakeLists.txt
│   ├── Readme.md                    # ReaderCGNS 接口、并发与构建说明
│   ├── CGNS.md                      # CGNS 文件格式与数据结构
│   ├── CGNS_API.md                  # CGNS 4.5.1 C API 开发参考
│   ├── include/ReaderAPI/          # ReaderCGNS 对外公开头文件
│   ├── src/                        # DLL reader 工厂导出
│   ├── Common/                     # CGNS 常量、拓扑与内部字段类型
│   ├── Core/                       # 文件生命周期、网格/场值读取与层次遍历
│   ├── Utils/                      # ReaderCGNS 内部日志工具
│   ├── tests/                      # ReaderCGNS 模块级 CTest 测试
│   └── 3rdparty/
│       └── cgns/                  # ReaderCGNS 使用的 CGNS 依赖
├── Utils/                          # 跨模块的头文件工具库
│   ├── BlockingQueue.hpp           # 线程安全阻塞队列
│   ├── ThreadPool.hpp              # 线程池
│   └── ...
├── Logger/                         # 基于 spdlog 的日志封装
├── 3rdparty/                       # 多个目标共用的第三方依赖
│   ├── boost/
│   ├── highfive/
│   ├── meojson/                    # JSON/JSON5 头文件库
│   ├── mio/
│   └── spdlog/
├── .clang-format                   # C/C++ 代码格式化配置
├── build/                          # CMake 构建目录（生成）
└── bin/<Debug|Release>/            # 可执行文件和动态库输出（生成）
```

`Core` 通过公开头获得 ABI 类型，使用 `LoadLibraryW`/`GetProcAddress` 解析 `ReaderCGNS.dll` 的 Create/Destroy 工厂，并通过 reader 虚接口完成文件检查和实例级日志配置；两者之间没有链接时依赖。根目录下的 `Utils` 和 `Logger` 为头文件形式的通用组件。`build/` 和 `bin/` 均为生成目录，不应在其中维护源代码。

## 模块文档

- [`Core/Readme.md`](Core/Readme.md)：命令行接口、运行流程、输出、依赖和日志生命周期。
- [`ReaderCGNS/Readme.md`](ReaderCGNS/Readme.md)：共享库能力、公开 API、回调并发约定和集成方式。
- [`ReaderCGNS/CGNS.md`](ReaderCGNS/CGNS.md)：CGNS 文件树、节点语义、元素类型与数据布局。
- [`ReaderCGNS/CGNS_API.md`](ReaderCGNS/CGNS_API.md)：仓库 CGNS 4.5.1 的完整 Mid-Level Library C API 开发参考。

## 第三方依赖

必需依赖均已 vendored 在仓库中：通用依赖位于根目录 `3rdparty/`，目标专用依赖分别位于 `Core/3rdparty/` 和 `ReaderCGNS/3rdparty/`。TBB 不随仓库交付；找到时 Core 额外链接 `TBB::tbb`，找不到时省略该链接。`std::execution::par` 的实际执行后端取决于标准库实现，不能据此判断是否串行执行；MSVC STL 的并行算法不依赖 TBB。

| 库        | 版本     | 链接方式                     | 用途                                    |
|----------|--------|--------------------------|---------------------------------------|
| Boost    | 1.91   | 静态库（`.lib`）              | `program_options`（CLI 解析）、`container` |
| CGNS     | 4.5.1  | 静态库（`CGNS::cgns_static`） | CGNS 网格/解文件读取                         |
| HDF5     | 2.1.1  | Core 动态链接；ReaderCGNS 私有静态依赖 | HighFive 数据后端与 CGNS 的 HDF5 存储后端 |
| HighFive | 3.3.0  | 头文件库                     | HDF5 C++ 封装                           |
| meojson  | vendored snapshot | 头文件库             | JSON/JSON5 解析与序列化                  |
| mio      | —      | 头文件库                     | 内存映射文件 I/O                            |
| spdlog   | 1.17.0 | 头文件库                     | 异步日志                                  |
| TBB      | 环境提供 | 可选动态/静态库               | 部分标准库实现的并行算法后端              |
