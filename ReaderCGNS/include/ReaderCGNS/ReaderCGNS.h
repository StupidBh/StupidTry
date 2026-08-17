#pragma once
#include "ReaderCGNS/ReaderCGNSTypes.hpp"
#include <string>

#ifdef _WIN32
    #ifdef READERCGNS_BUILD
        #define READER_CGNS_DLL __declspec(dllexport)
    #else
        #define READER_CGNS_DLL __declspec(dllimport)
    #endif
#else
    #define READER_CGNS_DLL
#endif

namespace ReaderCGNS {
    namespace Logger {
        /// Installs the process-wide log callback and waits for the previous callback to finish.
        /// The callback runs synchronously on the calling ReaderCGNS thread and may run concurrently
        /// on multiple threads. The context and string pointers are borrowed for the callback duration.
        /// Callback exceptions are caught by ReaderCGNS. Calling SetLogCallback or ClearLogCallback
        /// from inside the callback is rejected to avoid a self-wait.
        READER_CGNS_DLL bool SetLogCallback(ReaderCGNS_LogCallback callback, void* context = nullptr) noexcept;

        /// Removes the callback and waits for all callback invocations on other threads to finish.
        READER_CGNS_DLL bool ClearLogCallback() noexcept;

        /// Sets the lowest severity dispatched to the callback. Defaults to trace.
        READER_CGNS_DLL bool SetMinimumLogLevel(ReaderCGNS_LogLevel level) noexcept;
        READER_CGNS_DLL ReaderCGNS_LogLevel GetMinimumLogLevel() noexcept;
    }

    READER_CGNS_DLL bool info(const std::string& cgns_file_path);
}
