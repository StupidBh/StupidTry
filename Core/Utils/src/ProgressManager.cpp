#include "ProgressManager.h"

ProgressManager::ProgressManager(
    std::string_view sharedName,
    bool autoFlush,
    std::chrono::milliseconds flushInterval,
    std::string_view outputFile) :
    m_shared_name(sharedName),
    m_flush_interval(flushInterval),
    m_output_file(outputFile)
{
    init_shared_memory();

    if (autoFlush && !m_output_file.empty()) {
        // 启动后台刷新线程
        m_flush_thread = std::jthread([this](std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                std::this_thread::sleep_for(m_flush_interval);
                flush_to_file(m_output_file);
            }
        });
    }
}

ProgressManager::~ProgressManager()
{
    if (m_data) {
        UnmapViewOfFile(m_data);
        m_data = nullptr;
    }
    if (m_shared_hadle) {
        CloseHandle(m_shared_hadle);
        m_shared_hadle = nullptr;
    }
}

void ProgressManager::add_progress(LONG delta) noexcept
{
    ::InterlockedAdd(reinterpret_cast<volatile LONG*>(&m_data->progress), delta);
}

LONG ProgressManager::get_progress() const noexcept
{
    return m_data->progress.load(std::memory_order_relaxed);
}

void ProgressManager::flush_to_file(std::string_view filepath) const
{
    std::ofstream ofs(filepath.data(), std::ios::trunc);
    if (!ofs) {
        throw std::runtime_error("Failed to open file: " + std::string(filepath));
    }
    ofs << m_data->progress.load(std::memory_order_relaxed);
}

void ProgressManager::init_shared_memory()
{
    m_shared_hadle = ::CreateFileMappingA(
        INVALID_HANDLE_VALUE, // 基于内存
        nullptr,
        PAGE_READWRITE,
        0,
        sizeof(SharedData),
        m_shared_name.c_str());

    if (!m_shared_hadle) {
        throw std::runtime_error("CreateFileMapping failed: " + std::to_string(GetLastError()));
    }

    m_data = static_cast<SharedData*>(::MapViewOfFile(m_shared_hadle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData)));

    if (!m_data) {
        ::CloseHandle(m_shared_hadle);
        m_shared_hadle = nullptr;
        throw std::runtime_error("MapViewOfFile failed: " + std::to_string(GetLastError()));
    }

    // 如果是新创建的共享内存，初始化进度
    if (::GetLastError() != ERROR_ALREADY_EXISTS) {
        m_data->progress.store(0, std::memory_order_relaxed);
    }
}
