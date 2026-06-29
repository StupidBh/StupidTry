#include "Logger.h"

#include "cgnslib.h"

namespace ReaderCGNS::Logger {
    int cgns_catch_msg(int status, const std::filesystem::path& file, int line, const char* function)
    {
        const auto& filename = file.filename().string();
        switch (status) {
        case CG_OK   : return CG_OK;
        case CG_ERROR: {
            LogFormat(READER_CGNS_LOG_ERROR, filename.c_str(), line, function, "[CG_ERROR]: {}", cg_get_error());
            return CG_ERROR;
        }
        case CG_NODE_NOT_FOUND: {
            LogFormat(READER_CGNS_LOG_WARN, filename.c_str(), line, function, "[CG_NODE_NOT_FOUND]: {}", cg_get_error());
            return CG_NODE_NOT_FOUND;
        }
        case CG_INCORRECT_PATH: {
            LogFormat(READER_CGNS_LOG_WARN, filename.c_str(), line, function, "[CG_INCORRECT_PATH]: {}", cg_get_error());
            return CG_INCORRECT_PATH;
        }
        case CG_NO_INDEX_DIM: {
            LogFormat(READER_CGNS_LOG_WARN, filename.c_str(), line, function, "[CG_NO_INDEX_DIM]: {}", cg_get_error());
            return CG_NO_INDEX_DIM;
        }

        default: {
            LogFormat(READER_CGNS_LOG_WARN, filename.c_str(), line, function, "Unknown status.");
            return status;
        }
        }
    }
} // namespace ReaderCGNS::Logger
