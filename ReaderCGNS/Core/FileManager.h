#pragma once
#include "Logger.h"

class FileManager : public ReaderAPI::ReaderCGNS {
public:
    explicit FileManager() = default;
    ~FileManager() override;

    bool SetLogCallback(ReaderAPI::Logger::LogCallback callback, void* context) noexcept final;
    bool ClearLogCallback() noexcept final;

    bool Open(const std::string& cgns_file_path) final;
    void Close() final;
    bool IsOpen() const final;

    float GetVersion() const final;
    std::string GetSolverType() const final;

protected:
    int GetFileID() const noexcept;
    const std::string& GetFileName() const noexcept;
    ReaderAPI::Logger::LogDispatcher& GetLogDispatcher() const noexcept;

private:
    mutable ReaderAPI::Logger::LogDispatcher m_log_dispatcher;
    int m_file_id = 0;
    std::string m_cgns_file_path;
};
