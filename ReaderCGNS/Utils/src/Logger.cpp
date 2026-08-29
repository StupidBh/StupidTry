#include "Logger.h"
#include "ReaderAPI/ReaderCGNS.h"

#include <atomic>
#include <cstdio>
#include <mutex>

#include "cgnslib.h"

namespace {
    using LogCallback = ReaderAPI::Logger::ReaderCGNS_LogCallback;
    using LogLevel = ReaderAPI::Logger::ReaderCGNS_LogLevel;

    struct LogCallbackRegistry
    {
        std::mutex state_mutex;
        std::mutex update_mutex;
        std::atomic_size_t active_callbacks { 0 };
        std::atomic_bool enabled { false };
        std::atomic<LogLevel> minimum_level { LogLevel::READER_CGNS_LOG_TRACE };
        LogCallback callback = nullptr;
        void* context = nullptr;
    };

    LogCallbackRegistry& GetRegistry()
    {
        static LogCallbackRegistry registry;
        return registry;
    }

    thread_local std::size_t callback_depth = 0;

    constexpr bool IsValidLogLevel(const LogLevel level) noexcept
    {
        return level >= LogLevel::READER_CGNS_LOG_TRACE && level <= LogLevel::READER_CGNS_LOG_CRITICAL;
    }

    bool ReplaceLogCallback(const LogCallback callback, void* context) noexcept
    {
        if (callback_depth != 0) {
            return false;
        }

        auto& registry = GetRegistry();
        try {
            const std::scoped_lock update_lock(registry.update_mutex);
            {
                const std::scoped_lock state_lock(registry.state_mutex);
                registry.enabled.store(false, std::memory_order_release);
                registry.callback = nullptr;
                registry.context = nullptr;
            }

            auto active = registry.active_callbacks.load(std::memory_order_acquire);
            while (active != 0) {
                registry.active_callbacks.wait(active, std::memory_order_acquire);
                active = registry.active_callbacks.load(std::memory_order_acquire);
            }

            const std::scoped_lock state_lock(registry.state_mutex);
            registry.callback = callback;
            registry.context = callback != nullptr ? context : nullptr;
            registry.enabled.store(callback != nullptr, std::memory_order_release);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool StoreMinimumLogLevel(const LogLevel level) noexcept
    {
        if (!IsValidLogLevel(level)) {
            return false;
        }

        GetRegistry().minimum_level.store(level, std::memory_order_release);
        return true;
    }

    LogLevel LoadMinimumLogLevel() noexcept
    {
        return GetRegistry().minimum_level.load(std::memory_order_acquire);
    }
} // namespace

namespace ReaderAPI::Logger::detail {
    bool IsDispatchEnabled(const ReaderCGNS_LogLevel level) noexcept
    {
        const auto& registry = GetRegistry();
        return IsValidLogLevel(level) && registry.enabled.load(std::memory_order_acquire) &&
               level >= registry.minimum_level.load(std::memory_order_acquire);
    }

    void Dispatch(const ReaderCGNS_LogLevel level, const char* file, const int line, const char* message) noexcept
    {
        auto& registry = GetRegistry();
        // Formatting occurs before this call, so callback state and level must be checked again.
        if (!IsDispatchEnabled(level)) {
            return;
        }

        ReaderCGNS_LogCallback callback = nullptr;
        void* context = nullptr;
        try {
            const std::scoped_lock state_lock(registry.state_mutex);
            if (registry.callback == nullptr || level < registry.minimum_level.load(std::memory_order_acquire)) {
                return;
            }

            callback = registry.callback;
            context = registry.context;
            registry.active_callbacks.fetch_add(1, std::memory_order_acq_rel);
        }
        catch (...) {
            return;
        }

        ++callback_depth;
        try {
            callback(context, level, file != nullptr ? file : "", line, message != nullptr ? message : "");
        }
        catch (...) {
            std::fputs("ReaderCGNS log callback threw an exception.\n", stderr);
        }
        --callback_depth;

        if (registry.active_callbacks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            registry.active_callbacks.notify_all();
        }
    }

    int HandleCgnsStatus(const int status, const std::string_view call, const std::source_location location) noexcept
    {
        if (status == CG_OK) {
            return CG_OK;
        }

        ReaderCGNS_LogLevel level = READER_CGNS_LOG_ERROR;
        const char* status_name = nullptr;
        switch (status) {
        case CG_ERROR         : status_name = "CG_ERROR"; break;
        case CG_NODE_NOT_FOUND: {
            level = READER_CGNS_LOG_WARN;
            status_name = "CG_NODE_NOT_FOUND";
        } break;
        case CG_INCORRECT_PATH: {
            level = READER_CGNS_LOG_WARN;
            status_name = "CG_INCORRECT_PATH";
        } break;
        case CG_NO_INDEX_DIM: {
            level = READER_CGNS_LOG_WARN;
            status_name = "CG_NO_INDEX_DIM";
        } break;
        default: break;
        }

        const char* error_message = cg_get_error();
        if (error_message == nullptr) {
            error_message = "No CGNS error message.";
        }

        const int line = static_cast<int>(location.line());
        if (status_name != nullptr) {
            FormatAndDispatch(level, location.file_name(), line, "[{}] {}: {}", status_name, call, error_message);
        }
        else {
            FormatAndDispatch(level, location.file_name(), line, "[CGNS_STATUS={}] {}: {}", status, call, error_message);
        }
        return status;
    }
} // namespace ReaderAPI::Logger::detail

// Stable entry points for clients that load ReaderCGNS.dll without an import library.
extern "C" READER_API bool SetLogCallback(const ReaderAPI::Logger::ReaderCGNS_LogCallback callback, void* context) noexcept
{
    return ReplaceLogCallback(callback, context);
}

extern "C" READER_API bool ClearLogCallback() noexcept
{
    return ReplaceLogCallback(nullptr, nullptr);
}

extern "C" READER_API bool SetMinimumLogLevel(const ReaderAPI::Logger::ReaderCGNS_LogLevel level) noexcept
{
    return StoreMinimumLogLevel(level);
}

extern "C" READER_API ReaderAPI::Logger::ReaderCGNS_LogLevel GetMinimumLogLevel() noexcept
{
    return LoadMinimumLogLevel();
}
