#include "CgnsCore.h"
#include "CgnsUtils.h"

void cgns::InitLog(std::shared_ptr<spdlog::logger> log)
{
    dylog::Logger::get_instance().UpdateLog(log);
}

void cgns::OpenCGNS(const std::string& file_path)
{
    LOG_INFO("Open in read only: [{}]", file_path);

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
         LOG_INFO("The file is a invalid [{}] file, msg: {}", FileTypeName(cgns_file_type), cg_get_error());
         return;
     }

    int cg_file_id = -1;
    if (CG_INFO(cg_open(file_path.c_str(), CG_MODE_READ, &cg_file_id)) != CG_OK) {
        LOG_ERROR("Open [{}] failed: {}", file_path, cg_file_id);
        return;
    }

    float cg_file_version = 0.F;
    CG_INFO(cg_version(cg_file_id, &cg_file_version));

    int cg_file_precision = 0;
    CG_INFO(cg_precision(cg_file_id, &cg_file_precision));

    LOG_INFO(
        "{}:[{}] v{:.2f}, precision={}",
        cg_file_id,
        FileTypeName(cgns_file_type),
        cg_file_version,
        cg_file_precision);

    CG_INFO(cg_close(cg_file_id));
}
