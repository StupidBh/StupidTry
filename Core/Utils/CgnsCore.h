#pragma once

#include <string>

class CgnsCore {
    static int CG_INFO(int status);

public:
    CgnsCore() = default;
    CgnsCore(const std::string& cgns_file_path);
    ~CgnsCore();

    bool OpenCGNS();
    bool OpenCGNS(const std::string& cgns_file_path);

    void CloseCGNS();
    void info() const;

    bool IsOpen() const;

private:
    std::string m_cgns_file_path;
    int m_cg_file_id = 0;
};
