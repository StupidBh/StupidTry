#pragma once
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

namespace ReaderCGNS {
    namespace Logger {
        READER_CGNS_DLL void SetLogCallback(ReaderCGNS_LogCallback callback);
        READER_CGNS_DLL void ClearLogCallback();
    }

    READER_CGNS_DLL bool info(const std::string& cgns_file_path);
}
