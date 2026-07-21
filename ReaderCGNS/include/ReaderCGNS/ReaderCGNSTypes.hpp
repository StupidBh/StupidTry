#pragma once

namespace ReaderCGNS {
    namespace Logger {
        typedef enum ReaderCGNS_LogLevel
        {
            READER_CGNS_LOG_TRACE = 0,
            READER_CGNS_LOG_DEBUG = 1,
            READER_CGNS_LOG_INFO = 2,
            READER_CGNS_LOG_WARN = 3,
            READER_CGNS_LOG_ERROR = 4,
            READER_CGNS_LOG_CRITICAL = 5
        } ReaderCGNS_LogLevel;
typedef void (*ReaderCGNS_LogCallback)(int level, const char* file, int line, const char* message);
    }
} // namespace ReaderCGNS
