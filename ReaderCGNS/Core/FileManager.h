#pragma once
#include "Logger.h"
#include "ReaderAPI/ReaderApiBase.h"

#include <string>

class FileManager : public ReaderAPI::ReaderApiBase {
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
    LogDispatcher& GetLogDispatcher() const noexcept;

private:
    mutable LogDispatcher m_log_dispatcher;
    int m_file_id = 0;
    std::string m_cgns_file_path;
};
