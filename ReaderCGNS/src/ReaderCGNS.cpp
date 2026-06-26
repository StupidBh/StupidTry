#include "ReaderCGNS/ReaderCGNS.h"
#include "Logger.h"

namespace ReaderCGNS {
    namespace Logger {

        void SetLogCallback(ReaderCGNS_LogCallback callback)
        {
            reader_cgns::g_log_callback.store(callback, std::memory_order_release);
        }

        void ClearLogCallback()
        {
            reader_cgns::g_log_callback.store(nullptr, std::memory_order_release);
        }
    }
}
