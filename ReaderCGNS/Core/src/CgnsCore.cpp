#include "../CgnsCore.h"
#include "Logger.h"

#include "cgnslib.h"

namespace ReaderCGNS {
    CgnsCore::CgnsCore(const std::string& cgns_file_path) :
        m_cgns_file_path(cgns_file_path)
    {
    }

    CgnsCore::~CgnsCore()
    {
        this->CloseCGNS();
    }

    bool CgnsCore::IsOpen() const
    {
        return this->m_cg_file_id != 0;
    }

    bool CgnsCore::OpenCGNS()
    {
        LOG_INFO("Open in read only: [{}]", this->m_cgns_file_path);

        int cgns_file_type = -1;
        const int status = cg_is_cgns(this->m_cgns_file_path.c_str(), &cgns_file_type);
        auto FileTypeName = [](int file_type) -> const char* {
            switch (file_type) {
            case CG_FILE_ADF : return "ADF";
            case CG_FILE_ADF2: return "ADF2";
            case CG_FILE_HDF5: return "HDF5";
            default          : return "ERROR_FILE";
            }
        };
        if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
            LOG_INFO("The file is a invalid [{}] file, msg: {}", FileTypeName(cgns_file_type), cg_get_error());
            this->m_cg_file_id = 0;
            return false;
        }

        if (cg_open(this->m_cgns_file_path.c_str(), CG_MODE_READ, &this->m_cg_file_id) != CG_OK) {
            LOG_ERROR("Open [{}] failed: {}", this->m_cgns_file_path, cg_get_error());
            this->m_cg_file_id = 0;
            return false;
        }

        float cg_file_version = 0.F;
        int cg_file_precision = 0;
        CG_INFO(cg_version(this->m_cg_file_id, &cg_file_version));
        CG_INFO(cg_precision(this->m_cg_file_id, &cg_file_precision));
        LOG_INFO("[{}] v{:.2f}, precision={}, file_id{}", FileTypeName(cgns_file_type), cg_file_version, cg_file_precision, this->m_cg_file_id);
        return true;
    }

    bool CgnsCore::OpenCGNS(const std::string& cgns_file_path)
    {
        this->CloseCGNS();
        this->m_cgns_file_path = cgns_file_path;
        return this->OpenCGNS();
    }

    void CgnsCore::CloseCGNS()
    {
        if (this->m_cg_file_id != 0) {
            LOG_INFO("Close CGNS file: [{}].", this->m_cgns_file_path);
            CG_INFO(cg_close(this->m_cg_file_id));
            this->m_cg_file_id = 0;
        }
        this->m_cgns_file_path = "";
    }

} // namespace ReaderCGNS
