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
        READER_CGNS_DLL void SetLogCallback(ReaderCGNS_LogCallback callback);
        READER_CGNS_DLL void ClearLogCallback();
    }

    READER_CGNS_DLL bool info(const std::string& cgns_file_path);
}
