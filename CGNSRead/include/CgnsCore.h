#pragma once
#include "CGNSReadFramework.h"

#include "log/logger.hpp"

namespace cgns {
    CGNS_EXPORT_API void InitLog(std::shared_ptr<spdlog::async_logger> log);

    CGNS_EXPORT_API void OpenCGNS(const std::string& file_path);
}
