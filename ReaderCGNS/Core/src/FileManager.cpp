#include "FileManager.h"
#include "Logger.h"

#include "cgnslib.h"

FileManager::~FileManager()
{
    this->Close();
}

bool FileManager::Open(const std::string& cgns_file_path)
{
    LOG_INFO("Open in read only: [{}]", cgns_file_path);

    int cgns_file_type = -1;
    const int status = cg_is_cgns(cgns_file_path.c_str(), &cgns_file_type);
    auto FileTypeName = [](int file_type) -> const char* {
        switch (file_type) {
        case CG_FILE_ADF : return "ADF";
        case CG_FILE_ADF2: return "ADF2";
        case CG_FILE_HDF5: return "HDF5";
        default          : return "ERROR_FILE";
        }
    };
    if (status != CG_OK || cgns_file_type == CG_FILE_NONE) {
        LOG_INFO("[CG_ERROR] [{}] msg: {}", FileTypeName(cgns_file_type), cg_get_error());
        return false;
    }

    // int cg_file_id = 0;
    if (CG_INFO(cg_open(cgns_file_path.c_str(), CG_MODE_READ, &this->m_file_id)) != CG_OK) {
        return false;
    }

    float cg_file_version = 0.F;
    int cg_file_precision = 0;
    CG_INFO(cg_version(this->m_file_id, &cg_file_version));
    CG_INFO(cg_precision(this->m_file_id, &cg_file_precision));
    LOG_INFO("[{}] v{:.2f}, precision={}, file_id={}", FileTypeName(cgns_file_type), cg_file_version, cg_file_precision, this->m_file_id);

    this->m_cgns_file_path = cgns_file_path;
    return true;
}

void FileManager::Close()
{
    if (this->m_file_id != 0) {
        LOG_INFO("Close CGNS file: [{}].", this->m_cgns_file_path);
        CG_INFO(cg_close(this->m_file_id));
        this->m_file_id = 0;
        this->m_cgns_file_path.clear();
    }
}

bool FileManager::IsOpen() const
{
    return this->m_file_id != 0;
}

int FileManager::GetFileID() const noexcept
{
    return this->m_file_id;
}

const std::string& FileManager::GetFileName() const noexcept
{
    return this->m_cgns_file_path;
}
