#pragma once

namespace ReaderAPI::Logger {
    enum ReaderCGNS_LogLevel : int
    {
        READER_CGNS_LOG_TRACE = 0,
        READER_CGNS_LOG_DEBUG = 1,
        READER_CGNS_LOG_INFO = 2,
        READER_CGNS_LOG_WARN = 3,
        READER_CGNS_LOG_ERROR = 4,
        READER_CGNS_LOG_CRITICAL = 5
    };

    using ReaderCGNS_LogCallback = void (*)(void* context, ReaderCGNS_LogLevel level, const char* file, int line, const char* message);
} // namespace ReaderAPI::Logger
