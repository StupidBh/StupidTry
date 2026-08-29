#include "Logger.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <mutex>

#include "cgnslib.h"

namespace ReaderAPI::Logger {
    namespace {
        thread_local std::size_t callback_depth = 0;

        constexpr bool IsValidLogLevel(const LogLevel level) noexcept
        {
            return level >= LogLevel::READER_CGNS_LOG_TRACE && level <= LogLevel::READER_CGNS_LOG_CRITICAL;
        }

        class CallbackInvocationGuard final {
        public:
            explicit CallbackInvocationGuard(std::atomic_size_t& active_callbacks) noexcept :
                m_active_callbacks(active_callbacks)
            {
                ++callback_depth;
            }

            ~CallbackInvocationGuard() noexcept
            {
                --callback_depth;
                if (this->m_active_callbacks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    this->m_active_callbacks.notify_all();
                }
            }

            CallbackInvocationGuard(const CallbackInvocationGuard&) = delete;
            CallbackInvocationGuard& operator=(const CallbackInvocationGuard&) = delete;

        private:
            std::atomic_size_t& m_active_callbacks;
        };
    } // namespace

    LogDispatcher::~LogDispatcher() noexcept
    {
        this->ClearCallback();
    }

    bool LogDispatcher::SetCallback(const LogCallback callback, void* context) noexcept
    {
        if (callback_depth != 0 || callback == nullptr) {
            return false;
        }

        try {
            const std::scoped_lock update_lock(this->m_update_mutex);
            const std::scoped_lock state_lock(this->m_state_mutex);
            if (this->m_callback != nullptr) {
                return false;
            }

            this->m_callback = callback;
            this->m_context = context;
            this->m_enabled.store(true, std::memory_order_release);
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool LogDispatcher::ClearCallback() noexcept
    {
        if (callback_depth != 0) {
            return false;
        }

        try {
            const std::scoped_lock update_lock(this->m_update_mutex);
            {
                const std::scoped_lock state_lock(this->m_state_mutex);
                this->m_enabled.store(false, std::memory_order_release);
                this->m_callback = nullptr;
                this->m_context = nullptr;
            }

            auto active = this->m_active_callbacks.load(std::memory_order_acquire);
            while (active != 0) {
                this->m_active_callbacks.wait(active, std::memory_order_acquire);
                active = this->m_active_callbacks.load(std::memory_order_acquire);
            }
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool LogDispatcher::IsDispatchEnabled(const LogLevel level) const noexcept
    {
        return IsValidLogLevel(level) && this->m_enabled.load(std::memory_order_acquire);
    }

    void LogDispatcher::Dispatch(const LogLevel level, const char* file, const int line, const char* message) noexcept
    {
        // Formatting occurs before this call, so callback state must be checked again.
        if (!this->IsDispatchEnabled(level)) {
            return;
        }

        LogCallback callback = nullptr;
        void* context = nullptr;
        try {
            const std::scoped_lock state_lock(this->m_state_mutex);
            if (this->m_callback == nullptr) {
                return;
            }

            callback = this->m_callback;
            context = this->m_context;
            this->m_active_callbacks.fetch_add(1, std::memory_order_acq_rel);
        }
        catch (...) {
            return;
        }

        const CallbackInvocationGuard invocation_guard(this->m_active_callbacks);
        try {
            callback(context, level, file != nullptr ? file : "", line, message != nullptr ? message : "");
        }
        catch (...) {
            std::fputs("ReaderCGNS log callback threw an exception.\n", stderr);
        }
    }

    int LogDispatcher::HandleCgnsStatus(const int status, const std::string_view call, const std::source_location location) noexcept
    {
        if (status == CG_OK) {
            return CG_OK;
        }

        LogLevel level = READER_CGNS_LOG_ERROR;
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
} // namespace ReaderAPI::Logger
