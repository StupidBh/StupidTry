#pragma once
#include "ReaderCGNS/ReaderCGNS.h"

#include <string>
#include <filesystem>

namespace ReaderCGNS {
    class READER_CGNS_DLL CgnsCore {
        static int CG_INFO(int status, const std::filesystem::path& file, int line);
        static constexpr int CGNS_MAX_NAME = 256;

    public:
        CgnsCore() = default;
        CgnsCore(const std::string& cgns_file_path);
        ~CgnsCore();

        bool IsOpen() const;
        bool OpenCGNS();
        bool OpenCGNS(const std::string& cgns_file_path);

        void CloseCGNS();
        void info();

    private:
        std::string m_cgns_file_path;
        int m_cg_file_id = 0;
    };
} // namespace ReaderCGNS
