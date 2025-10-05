#pragma once
#include "CGNSReadFramework.h"

namespace cgns {
    CGNS_EXPORT_API void InitLog(std::shared_ptr<spdlog::logger> log);

    CGNS_EXPORT_API void OpenCGNS(const std::string& file_path);
}
