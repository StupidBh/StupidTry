# StupidBhh

## 语言标准

- **C++23**（`CMAKE_CXX_STANDARD 23`，`CMAKE_CXX_STANDARD_REQUIRED ON`）
- **CMake 4.0+**
- **首选生成器：Visual Studio 18 2026（x64）**

## 构建与测试

优先使用 Visual Studio 18 安装目录中附带的 CMake、MSBuild 和 LLVM 工具；如果系统 `PATH` 中的 CMake 不支持 VS 18 生成器，应先定位 VS 18 自带的 CMake。

```powershell
cmake -S . -B build/Debug -G "Visual Studio 18 2026" -A x64
cmake --build build/Debug --config Debug
ctest --test-dir build/Debug -C Debug
cmake --build build/Debug --config Release
ctest --test-dir build/Debug -C Release
```

## 工程目录

```text
StupidTry/
├── CMakeLists.txt                  # 根 CMake 入口，配置语言标准、输出目录和子工程
├── Core/                           # 主程序 Core 可执行文件
│   ├── CMakeLists.txt
│   ├── Readme.md                    # Core 架构、运行与开发说明
│   ├── src/
│   │   └── Main.cpp                # 程序入口和功能示例
│   ├── Common/                     # 参数处理、全局配置等通用实现
│   │   ├── Functions.h
│   │   ├── SingletonData.h
│   │   └── src/
│   ├── Utils/                      # Core 专用的 HDF5、文件 I/O 和集成工具
│   │   ├── HighFiveUtils.hpp
│   │   ├── MioReader.h
│   │   ├── ReaderCGNSLogGuard.h    # ReaderCGNS 到 spdlog 的作用域适配
│   │   └── src/
│   └── 3rdparty/
│       └── hdf5/                  # Core 使用的 HDF5 依赖
├── ReaderCGNS/                     # CGNS 文件读取与检查共享库
│   ├── CMakeLists.txt
│   ├── Readme.md                    # ReaderCGNS 接口、并发与构建说明
│   ├── CGNS.md                      # CGNS 数据结构与 C API 指南
│   ├── include/ReaderCGNS/         # ReaderCGNS 对外公开头文件
│   ├── src/                        # 公开 API 实现
│   ├── Core/                       # CGNS 核心解析逻辑
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

`Core` 依赖 `ReaderCGNS` 共享库；根目录下的 `Utils` 和 `Logger` 为头文件形式的通用组件。`build/` 和 `bin/` 均为生成目录，不应在其中维护源代码。

## 模块文档

- [`Core/Readme.md`](Core/Readme.md)：命令行接口、运行流程、输出、依赖和日志生命周期。
- [`ReaderCGNS/Readme.md`](ReaderCGNS/Readme.md)：共享库能力、公开 API、回调并发约定和集成方式。
- [`ReaderCGNS/CGNS.md`](ReaderCGNS/CGNS.md)：CGNS 数据结构、元素类型与 Mid-Level Library C API 指南。

## 第三方依赖

所有依赖均已 vendored 在仓库中：通用依赖位于根目录 `3rdparty/`，目标专用依赖分别位于 `Core/3rdparty/` 和 `ReaderCGNS/3rdparty/`。

| 库        | 版本     | 链接方式                     | 用途                                    |
|----------|--------|--------------------------|---------------------------------------|
| Boost    | 1.91   | 静态库（`.a`）                | `program_options`（CLI 解析）、`container` |
| CGNS     | 4.5.1  | 静态库（`CGNS::cgns_static`） | CGNS 网格/解文件读取                         |
| HDF5     | 2.1.1  | 动态库（`hdf5::hdf5-shared`） | CGNS 的 HDF5 存储后端                      |
| HighFive | 3.3.0  | 头文件库                     | HDF5 C++ 封装                           |
| meojson  | vendored snapshot | 头文件库             | JSON/JSON5 解析与序列化                  |
| mio      | —      | 头文件库                     | 内存映射文件 I/O                            |
| spdlog   | 1.17.0 | 头文件库                     | 异步日志                                  |
