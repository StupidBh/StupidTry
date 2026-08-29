# ReaderCGNS 实例级日志回调改造方案

> 本文是临时设计文档，用于记录 ReaderCGNS 日志回调从 DLL 进程级全局配置迁移到 reader 实例级配置的方案。实现完成后应将稳定契约同步到 `Readme.md`，并删除本文。

## 1. 改造目标

将 ReaderCGNS 的日志回调从“DLL 进程级全局配置”改为“每个 `ReaderCGNS` 对象独立配置”。

DLL 只负责保存并调用宿主设置的回调，不认识 spdlog 等具体日志框架，也不解释、访问或释放 EXE 提供的上下文对象。

改造完成后的关系为：

```text
readerA -> dispatcher A -> callback/context A
readerB -> dispatcher B -> callback/context B
```

## 2. 公开接口

在 `include/ReaderAPI/ReaderCGNS.h` 的 `ReaderAPI::ReaderCGNS` 中增加实例级日志接口：

```cpp
virtual bool SetLogCallback(
    Logger::LogCallback callback,
    void* context) noexcept = 0;

virtual bool ClearLogCallback() noexcept = 0;
```

日志注册采用“先清理、后重新绑定”的状态模型，不支持直接替换：

- 未绑定时，`SetLogCallback()` 传入非空回调会完成绑定并返回 `true`；
- `SetLogCallback(nullptr, context)` 是无效调用，返回 `false` 且不改变当前状态；
- 已绑定且尚未清理时，再次调用 `SetLogCallback()` 返回 `false`，原有 callback/context 保持不变；
- 已绑定时，`ClearLogCallback()` 先停止新的分发，等待已经进入的回调结束，再清除绑定并返回 `true`；
- 未绑定时，`ClearLogCallback()` 是幂等操作并返回 `true`；
- 成功清理后，可以再次调用 `SetLogCallback()` 建立新绑定。

`false` 是注册失败的完整通知。DLL 不通过日志回调报告注册错误，避免未绑定时没有通知通道以及回调内递归派发；调用方可以根据返回值使用自己的日志系统记录警告。

保留日志回调类型：

```cpp
using LogCallback = void (*)(
    void* context,
    LogLevel level,
    const char* file,
    int line,
    const char* message);
```

`context` 是可选的不透明宿主状态：

- 当前 Core 不需要绑定日志对象，可以传入 `nullptr`；
- 其他调用方可以传入测试收集器、GUI 对象、消息队列或日志适配器；
- DLL 只在调用回调时原样传回该指针；
- 指针所指对象由调用方拥有，其生命周期必须覆盖回调注册期，直到成功的 `ClearLogCallback()` 或 `DestroyReaderCGNS()` 返回。

删除以下函数指针类型：

```cpp
SetLogCallbackFunc
ClearLogCallbackFunc
```

日志配置不再通过 `GetProcAddress` 调用。

## 3. DLL 导出

ReaderCGNS DLL 只保留两个稳定 C 导出：

```text
CreateReaderCGNS
DestroyReaderCGNS
```

删除进程级日志导出：

```text
SetLogCallback
ClearLogCallback
```

调用方加载 DLL 后只解析工厂函数，其他操作全部通过具体 `ReaderCGNS` 对象的虚接口完成。

## 4. 实例级日志状态

删除 `Utils/src/Logger.cpp` 中的静态全局 `LogCallbackRegistry`，新增内部 `LogDispatcher`。每个 reader 对象持有一份 dispatcher：

```cpp
class LogDispatcher final {
public:
    ~LogDispatcher() noexcept;

    bool SetCallback(LogCallback callback, void* context) noexcept;
    bool ClearCallback() noexcept;

    void Dispatch(
        LogLevel level,
        const char* file,
        int line,
        const char* message) noexcept;

private:
    std::mutex m_state_mutex;
    std::mutex m_update_mutex;
    std::atomic_size_t m_active_callbacks { 0 };
    bool m_enabled = false;
    LogCallback m_callback = nullptr;
    void* m_context = nullptr;
};
```

`SetCallback()` 只负责从未绑定状态建立绑定，不替换现有 callback/context。并发调用由 `m_update_mutex` 串行化；两个线程同时对同一 reader 注册时，只有一个调用可以成功。

`LogDispatcher` 析构时调用 `ClearCallback()`，保证 `DestroyReaderCGNS()` 返回前完成实例回调清理。调用方仍必须遵守销毁前停止该 reader 所有 API 调用的前置条件。

保留一个不持有 callback/context 的模块级 `thread_local callback_depth`，只用于判断当前线程是否正在执行任意 ReaderCGNS 日志回调。`Dispatch()` 在调用宿主回调前增加深度，并以 RAII 保证在正常返回或异常时恢复；Set/Clear 在获取 dispatcher 锁之前检查该值，非零时直接返回 `false` 且不改变目标实例状态。该守卫不属于日志配置状态，不影响各 reader dispatcher 的实例隔离。

由 `FileManager` 持有该对象，并实现公开虚接口：

```cpp
class FileManager : public ReaderAPI::ReaderCGNS {
public:
    bool SetLogCallback(LogCallback callback, void* context) noexcept final;
    bool ClearLogCallback() noexcept final;

private:
    LogDispatcher m_log_dispatcher;
    int m_file_id = 0;
    std::string m_cgns_file_path;
};
```

## 5. DLL 内部日志调用

当前 `LOG_INFO`、`LOG_WARN` 和 `CG_INFO` 等入口访问全局注册表，必须改为通过当前 reader 实例分发。例如：

```cpp
this->LogInfo(...);
this->HandleCgnsStatus(...);
```

也可以保留内部宏，但宏必须调用当前对象的 `LogDispatcher`，不得再次引入进程级日志状态。

`FileManager.cpp` 和 `CgnsCore.cpp` 中的日志最终都进入当前对象自己的 dispatcher。

当前没有公开最低日志级别控制接口，因此移除全局 `minimum_level`。日志级别过滤交给 EXE 提供的回调处理。

## 6. Core 中的 AnalysisCGNS

删除 `ReaderCGNSLogGuard`，将日志适配直接纳入 `AnalysisCGNS`。

`AnalysisCGNS` 增加静态回调：

```cpp
static void LogCallback(
    void* context,
    ReaderAPI::Logger::LogLevel level,
    const char* file,
    int line,
    const char* message);
```

初始化顺序调整为：

```text
加载 ReaderCGNS.dll
-> 解析 CreateReaderCGNS/DestroyReaderCGNS
-> 创建 ReaderCGNS 对象
-> reader->SetLogCallback(AnalysisCGNS::LogCallback, nullptr)
```

日志回调是可选能力。Core 必须检查 `SetLogCallback()` 的返回值并通过自身 logger 记录警告，但注册失败不使 reader 初始化失败；`AnalysisCGNS::operator bool()` 仍只检查 module 和 reader。

当前 Core 的回调直接调用 `spdlog::default_logger()`，不再把 `LOG.get()` 裸指针交给 DLL。

析构顺序必须明确为：

```cpp
AnalysisCGNS::~AnalysisCGNS() noexcept
{
    if (this->m_reader != nullptr) {
        if (this->m_reader->IsOpen()) {
            this->m_reader->Close();
        }

        this->m_reader->ClearLogCallback();
        this->m_reader.reset();
    }
}
```

先关闭文件以保留关闭过程日志，再清除回调、销毁 reader，最后卸载 DLL。

成员声明顺序应保证 `m_reader` 先于 `m_module_guard` 销毁；析构函数中的显式 `reset()` 进一步固定该顺序。

## 7. 删除 ReaderCGNSLogGuard

删除：

```text
Core/ReaderCGNS/ReaderCGNSLogGuard.h
Core/ReaderCGNS/src/ReaderCGNSLogGuard.cpp
```

同步调整：

- 从 `Core/CMakeLists.txt` 移除源文件；
- 从 `AnalysisCGNS.h` 删除前置声明和 `m_log_guard`；
- 从 `AnalysisCGNS.cpp` 删除 guard 创建和状态检查；
- 将日志级别到 spdlog 级别的转换移入 `AnalysisCGNS::LogCallback`；
- `AnalysisCGNS::operator bool()` 只检查 module 和 reader；
- 更新 README 中的目录、动态导出和日志生命周期说明。

以后若通过 DLL 派发另一类任务，应创建对应的宿主类。该类自行创建 reader、设置回调和管理生命周期，不再复用独立的日志 guard。

## 8. 并发与生命周期约定

每个 `LogDispatcher` 独立保证：

- 清理 reader A 不影响 reader B；
- reader A 的注册或清理操作不暂停 reader B；
- 已绑定时重复注册返回 `false`，并保留原 callback/context；
- 空回调注册返回 `false`，并且不改变当前状态；
- `ClearLogCallback()` 等待该 reader 已经进入的回调结束；
- 未绑定时调用 `ClearLogCallback()` 返回 `true`；
- 在任意 ReaderCGNS 日志回调执行期间，从当前线程对任意 reader 调用 Set/Clear 都返回 `false` 且不改变目标实例状态，避免递归分发、自等待和跨实例互等；
- 回调抛出的异常不会越过 DLL 边界；
- `context` 必须存活到成功的 `ClearLogCallback()` 或 `DestroyReaderCGNS()` 返回。

日志回调在触发日志的线程上同步执行，同一 callback 可能被多个 reader 调用并发进入，调用方必须保证回调线程安全。`context`、`file` 和 `message` 都是借用数据，其中 `file` 和 `message` 仅在本次回调期间有效。

回调执行期间的拒绝规则优先于未绑定状态下 Clear 的幂等规则；即使目标 reader 尚未绑定，从回调线程调用其 `ClearLogCallback()` 仍返回 `false`。

调用方必须在销毁 reader 前停止所有针对该对象的 API 调用。reader 析构时清理 dispatcher 是内部防御措施，不提供与并发成员调用安全销毁的保证；若调用方未显式清理回调，context 必须存活到 `DestroyReaderCGNS()` 返回。

## 9. 分支与测试安排

生产改造提交到 `dev`，包括公开接口、实例 dispatcher、Core 集成、构建文件和用户文档。

将 `dev` 合入 `test` 后，在 `test` 分支增加：

- 两个 reader 分别收到自己的日志；
- 清理 reader A 不影响 reader B；
- 已绑定时重复注册返回 `false`，并保留原 callback/context；
- 空回调注册返回 `false`，并且不改变当前状态；
- 清理后可以重新注册；
- 未绑定时清理返回 `true`；
- 两个线程同时对同一 reader 注册时只有一个成功；
- 清理操作等待正在执行的回调；
- 任意 reader 的回调内，对任意 reader 调用 Set/Clear 都返回 `false`，并且不改变目标实例状态；
- reader 销毁后不再触发回调；
- 回调抛出异常时不跨越 DLL 边界；
- Debug 和 Release 的动态加载及 CTest 验证。

测试使用测试夹具和局部日志收集器，不保留生产 `ReaderCGNSLogGuard`。

## 10. 兼容性

本次调整会改变 `ReaderAPI::ReaderCGNS` 的虚函数表，并删除两个 DLL 导出，属于 ABI 破坏性变更。

新的 `ReaderCGNS.h` 与 `ReaderCGNS.dll` 必须配套交付。实现完成后必须同步更新：

- `ReaderCGNS/Readme.md`；
- `Core/Readme.md`；
- 根目录 `README.md` 中受影响的结构或动态加载说明；
- 测试分支中的动态导出和多实例日志测试。

头文件与 DLL 配套属于工程交付约定，本次不增加 ABI 版本导出或运行时版本校验。
