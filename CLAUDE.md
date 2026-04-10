# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 构建系统

这是一个使用 Visual Studio 2022 (v143 toolset) 和 MSBuild 的 Windows C++ 项目。可通过 Visual Studio 或命令行构建：

```
msbuild StupidTry.sln /p:Configuration=Release /p:Platform=x64
```

输出二进制文件位于 `bin\[Platform]\[Configuration]\` 目录：
- `stupid-app.exe` - 主可执行文件（Core 项目）
- `stupid-h5.dll` - HDF5 读取器库（ReaderH5 项目）
- `stupid-cgns.dll` - CGNS 读取器库（CGNSRead 项目）

## 代码格式化

项目使用 `.clang-format`，基于 WebKit 风格的自定义配置。关键点：
- 标准：C++20（C 文件使用 C17）
- 列限制：120
- 缩进：4 个空格，不使用制表符
- 指针/引用对齐：左对齐（`Type* ptr` 而非 `Type *ptr`）
- 大括号：自定义规则 - 函数、结构体、联合体、枚举、extern 块另起一行
- 二进制打包：禁用（参数每行一个）
- 多行字符串和模板声明前总是换行

使用命令格式化：`clang-format -i [文件名]`

## 项目架构

解决方案由四个主要项目组成：

### Core（主应用程序）
- 入口点：`Core/src/Main.cpp`
- 包含公共工具（`Core/Common/`）和数据 I/O 工具（`Core/Utils/`）
- 依赖 ReaderH5 和 CGNSRead 库
- 依赖 HDF5 库和 Boost（program_options）

### Utils（仅头库）
- `Utils/Utils.hpp` - 类型特征、向量操作（`ShrinkVector`, `AppendVector`, `CreateVector`, `DeepClear`）
- `Utils/SingletonHolder.hpp` - 单例模式实现；使用 `SINGLETON_CLASS(类名)` 宏
- `Utils/ScopedTimer.hpp` - RAII 计时器用于测量执行时间；使用 `SCOPED_TIMER(消息)` 宏

### Logger（DLL）
- `Logger/include/log/logger.hpp` - spdlog 的异步日志包装器
- 同时输出到控制台和每日轮转日志文件（`logs/[文件名].log`）
- 提供 `LOG_INFO`, `LOG_WARN`, `LOG_DEBUG`, `LOG_ERROR` 宏
- Windows 上配置 UTF-8 控制台输出

### ReaderH5（DLL）
- HDF5 文件读取功能
- 头文件：`Core/Utils/HDF5.hpp`（HDF5 C++ 包装器），`Core/Utils/HighFive.hpp`（HighFive 库）
- 使用 `HDF5Utils::WriteDataSet` 模板写入数据集
- 链接：HDF5 C 和 C++ 库、libaec、libszip、zlib、shlwapi

### CGNSRead（DLL）
- CFD 数据的 CGNS 文件格式支持
- 链接：CGNS 库（依赖 HDF5）
- 导出 `cgns::InitLog()` 和 `cgns::OpenCGNS()` 函数

## 关键模式

### 单例模式
使用 `SingletonHolder` 基类和 `SINGLETON_CLASS` 宏：
```cpp
class MyClass : public utils::SingletonHolder<MyClass> {
    SINGLETON_CLASS(MyClass);
    // ...
};

// 通过宏访问
auto& instance = MyClass::get_instance();
```

### 全局单例
- `LOG` - 返回 spdlog 默认日志记录器
- `SINGLE_DATA` - `stupid::SingletonData` 实例，用于命令行参数
- `INPUT_PATH` - 从参数获取输入路径的便捷宏
- `WORK_DIR` - 从参数获取工作目录的便捷宏

### 计时
使用作用域计时器进行性能测量：
```cpp
SCOPED_TIMER("操作名称");
SCOPED_TIMER_LOG("操作名称");  // 同时记录耗时
```

### 类型概念
代码库大量使用 C++20 概念：
- `utils::VectorType` - 匹配 `std::vector<T>`
- `utils::ArrayType` - 匹配 `std::array<T, N>`
- `HDF5Node` - 匹配派生自 HDF5 组类型的类
- `HDF5Writable` - 结合向量/数组类型与 HDF5 节点

### 第三方库
- **HDF5**: 位于 `3rdparty/hdf5/`。通过原生 C++ API (`H5::`) 和 HighFive 头库使用
- **Boost**: 位于 `3rdparty/boost/`。在 Core 中用于 program_options
- **spdlog**: 通过 Logger 包装器使用（无需直接使用 spdlog）
- **mio**: 内存映射 I/O 库，位于 `Core/3rdparty/mio/`
- **HighFive**: 现代 C++ HDF5 包装器，头库形式位于 `Core/3rdparty/highfive/`

## 文件组织
- `3rdparty/` - 外部依赖（HDF5、Boost、头文件）
- `bin/` - 构建输出目录
- `Utils/` - 仅头工具类
- `[项目]/include/` - 公开头文件
- `[项目]/src/` - 源文件
- `[项目]/Core/` - 项目特定工具代码

## 预处理器定义
- `H5_BUILT_AS_DYNAMIC_LIB` - HDF5 动态链接必需
- 导出宏：`CGNS_EXPORT_API`, `H5_EXPORT_API`（根据 `STUPID_EXPORT_LIBRARY` 决定 dllexport/dllimport）
