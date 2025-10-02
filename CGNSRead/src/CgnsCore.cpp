#include "CgnsCore.h"

#include "cgns/cgnslib.h"

void cgns::InitLog(std::shared_ptr<spdlog::async_logger> log)
{
    spdlog::set_default_logger(log);
}

void cgns::OpenCGNS(const std::string& file_path)
{
    int cgns_file_type = -1;
    int status = cg_is_cgns(file_path.c_str(), &cgns_file_type);
    auto FileTypeName = [](int file_type) -> const char* {
        switch (file_type) {
        // case CG_FILE_NONE: return "ERROR_FILE";
        case CG_FILE_ADF : return "ADF";
        case CG_FILE_ADF2: return "ADF2";
        case CG_FILE_HDF5: return "HDF5";
        default          : return "ERROR_FILE";
        }
    };
    if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
        LOG_INFO("The [{}] is a invalid [{}] file, msg: {}", file_path, FileTypeName(cgns_file_type), cg_get_error());
        return;
    }

    int cg_file_id = -1;
    status = cg_open(file_path.c_str(), CG_MODE_READ, &cg_file_id);
    if (status != CG_OK) {
        LOG_INFO("Open [{}] Faild: {}", file_path, cg_get_error());
        return;
    }

    float cg_file_version = 0.F;
    cg_version(cg_file_id, &cg_file_version);

    int cg_file_precision = 0;
    cg_precision(cg_file_id, &cg_file_precision);

    LOG_INFO(
        "[{}] {}, v{:.2f}, precision={}",
        FileTypeName(cgns_file_type),
        file_path,
        cg_file_version,
        cg_file_precision);
}
