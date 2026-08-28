#pragma once
#include "ReaderCGNS/ReaderCGNS.h"

class FileManager : public ReaderAPI::ReaderCGNS {
public:
    explicit FileManager() = default;
    ~FileManager() override;

    bool Open(const std::string& cgns_file_path) final;
    void Close() final;
    bool IsOpen() const final;

protected:
    int GetFileID() const noexcept;
    const std::string& GetFileName() const noexcept;

private:
    int m_file_id = 0;
    std::string m_cgns_file_path;
};
