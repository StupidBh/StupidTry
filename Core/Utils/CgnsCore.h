#pragma once
#include <string>
#include <filesystem>

class CgnsCore {
    static int CG_INFO(int status, const std::filesystem::path& file, int line);
    static constexpr int CGNS_MAX_NAME = 256;

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
