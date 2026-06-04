# StupidBhh

## 语言标准

- **C++20**（`CMAKE_CXX_STANDARD 20`，`CMAKE_CXX_STANDARD_REQUIRED ON`）
- **CMake 4.0+**

## 第三方依赖（`3rdparty/`）

所有依赖均已 vendored 在 `3rdparty/` 目录下。

| 库 | 版本 | 链接方式 | 用途 |
|----|------|----------|------|
| Boost | 1.91 | 动态库（DLL） | `program_options`（CLI 解析）、`container` |
| CGNS | 4.5.1 | 静态库（`CGNS::cgns_static`） | CGNS 网格/解文件读取 |
| HDF5 | 2.1.1 | 静态库（`hdf5::hdf5-static`） | CGNS 的 HDF5 存储后端 |
| HighFive | 3.3.0 | 头文件库 | HDF5 C++ 封装 |
| mio | — | 头文件库 | 内存映射文件 I/O |
| spdlog | 1.17.0 | 头文件库 | 异步日志 |
