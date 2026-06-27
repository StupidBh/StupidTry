#include "ReaderCGNS/ReaderCGNS.h"
#include "Logger.h"

namespace ReaderCGNS::Logger {
    void SetLogCallback(ReaderCGNS_LogCallback callback)
    {
        g_log_callback.store(callback, std::memory_order_release);
    }

    void ClearLogCallback()
    {
        g_log_callback.store(nullptr, std::memory_order_release);
    }
}
