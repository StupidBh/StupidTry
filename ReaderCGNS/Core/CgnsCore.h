#pragma once
#include "ReaderCGNS/ReaderCGNS.h"

namespace ReaderCGNS {
    class CgnsCore {
    public:
        CgnsCore() = default;
        CgnsCore(const std::string& cgns_file_path);
        ~CgnsCore();

        bool IsOpen() const;
        bool OpenCGNS();
        bool OpenCGNS(const std::string& cgns_file_path);
        void CloseCGNS();

    private:
        std::string m_cgns_file_path;
        int m_cg_file_id = 0;
    };
} // namespace ReaderCGNS
