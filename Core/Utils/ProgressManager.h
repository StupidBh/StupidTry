#pragma once
#include <windows.h>
#include <string_view>
#include <string>
#include <fstream>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>
#include <stop_token>

/**
 * @brief 跨进程共享进度管理器
 *
 * 使用 Windows 内存映射文件 (CreateFileMapping + MapViewOfFile)
 * 共享一个整数，支持多进程原子累加。
 * 可选开启后台线程定时写文件。
 */
class ProgressManager {
    ProgressManager(const ProgressManager&) = delete;
    ProgressManager& operator=(const ProgressManager&) = delete;

public:
    /**
     * @brief 构造函数
     * @param sharedName     跨进程共享内存名称，建议加 "Global\\" 前缀
     * @param autoFlush      是否启用后台自动写文件
     * @param flushInterval  刷新间隔（毫秒）
     * @param outputFile     自动写入的文件路径（仅 autoFlush=true 时使用）
     */
    ProgressManager(
        std::string_view sharedName,
        bool autoFlush = false,
        std::chrono::milliseconds flushInterval = std::chrono::seconds(1),
        std::string_view outputFile = {});

    ~ProgressManager();

    /**
     * @brief 增加进度（跨进程原子）
     * @param delta 增量
     */
    void add_progress(LONG delta) noexcept;

    /**
     * @brief 获取当前进度值
     */
    [[nodiscard]] LONG get_progress() const noexcept;

    /**
     * @brief 将当前进度写入文件
     * @param filepath 输出文件路径
     */
    void flush_to_file(std::string_view filepath) const;

private:
    struct SharedData
    {
        std::atomic<LONG> progress { 0 }; ///< 跨进程共享的进度值
    };

    void init_shared_memory(); ///< 初始化跨进程共享内存

private:
    std::string m_shared_name;
    std::chrono::milliseconds m_flush_interval;
    std::string m_output_file;

    HANDLE m_shared_hadle = nullptr; ///< 共享内存句柄
    SharedData* m_data = nullptr;    ///< 指向共享数据的指针
    std::jthread m_flush_thread;     ///< 后台刷新线程（C++20）
};
