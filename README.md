# StupidBhh

## 工具链与语言标准

- **C++23**（由 `Core` 和 `ReaderCGNS` 子项目设置，且要求严格满足）
- **C17**（由两个子项目为 C 源设置）
- **CMake 4.0+**
- **首选生成器：Visual Studio 18 2026（x64）**
- **MSVC 使用 `/EHsc` 启用标准 C++ 异常展开语义**
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

## 工程目录

```text
StupidTry/
├── CMakeLists.txt                  # 根 CMake 入口，配置 MSVC 选项、输出目录和子工程
├── Core/                           # 主程序 Core 可执行文件
│   ├── CMakeLists.txt
│   ├── Readme.md                    # Core 架构、运行与开发说明
│   ├── src/
│   │   └── Main.cpp                # 程序入口和功能示例
│   ├── Common/                     # 参数处理、全局配置等通用实现
│   │   ├── Functions.h             # 仅依赖标准库的字符串工具
│   │   ├── SingletonData.h
│   │   ├── WindowsFunctions.h      # Win32 编码、进程、环境和路径工具
│   │   └── src/
│   ├── ReaderCGNS/                 # ReaderCGNS 的应用侧集成
│   │   ├── ReaderCGNSLogGuard.h    # ReaderCGNS 到 spdlog 的作用域适配
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
│   ├── CGNS.md                      # CGNS 数据结构与 C API 指南
│   ├── include/ReaderAPI/          # ReaderCGNS 对外公开头文件
│   ├── src/                        # DLL reader 工厂导出
│   ├── Core/                       # 文件生命周期与 CGNS 层次遍历
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

`Core` 通过公开头获得 ABI 类型，并使用 `LoadLibraryW`/`GetProcAddress` 调用 `ReaderCGNS.dll`，两者之间没有链接时依赖。根目录下的 `Utils` 和 `Logger` 为头文件形式的通用组件。`build/` 和 `bin/` 均为生成目录，不应在其中维护源代码。

## 模块文档

- [`Core/Readme.md`](Core/Readme.md)：命令行接口、运行流程、输出、依赖和日志生命周期。
- [`ReaderCGNS/Readme.md`](ReaderCGNS/Readme.md)：共享库能力、公开 API、回调并发约定和集成方式。
- [`ReaderCGNS/CGNS.md`](ReaderCGNS/CGNS.md)：CGNS 数据结构、元素类型与 Mid-Level Library C API 指南。

## 第三方依赖

必需依赖均已 vendored 在仓库中：通用依赖位于根目录 `3rdparty/`，目标专用依赖分别位于 `Core/3rdparty/` 和 `ReaderCGNS/3rdparty/`。TBB 不随仓库交付，仅作为可选的环境依赖；找不到时相关标准并行算法会退化为串行执行。

| 库        | 版本     | 链接方式                     | 用途                                    |
|----------|--------|--------------------------|---------------------------------------|
| Boost    | 1.91   | 静态库（`.lib`）              | `program_options`（CLI 解析）、`container` |
| CGNS     | 4.5.1  | 静态库（`CGNS::cgns_static`） | CGNS 网格/解文件读取                         |
| HDF5     | 2.1.1  | Core 动态链接；ReaderCGNS 私有静态依赖 | HighFive 数据后端与 CGNS 的 HDF5 存储后端 |
| HighFive | 3.3.0  | 头文件库                     | HDF5 C++ 封装                           |
| meojson  | vendored snapshot | 头文件库             | JSON/JSON5 解析与序列化                  |
| mio      | —      | 头文件库                     | 内存映射文件 I/O                            |
| spdlog   | 1.17.0 | 头文件库                     | 异步日志                                  |
| TBB      | 环境提供 | 可选动态/静态库               | `std::execution::par` 并行后端              |
