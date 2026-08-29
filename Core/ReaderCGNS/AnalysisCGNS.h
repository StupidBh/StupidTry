#pragma once
#include "ReaderAPI/ReaderCGNS.h"

#include <filesystem>
#include <memory>

class ModuleGuard;
class ReaderCGNSLogGuard;

class AnalysisCGNS final {
    explicit AnalysisCGNS(const std::filesystem::path& library_path);

public:
    AnalysisCGNS();
    ~AnalysisCGNS() noexcept;

    AnalysisCGNS(const AnalysisCGNS&) = delete;
    AnalysisCGNS& operator=(const AnalysisCGNS&) = delete;
    AnalysisCGNS(AnalysisCGNS&&) = delete;
    AnalysisCGNS& operator=(AnalysisCGNS&&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] bool Analyze(const std::string& cgns_file_path) const;

private:
    struct ReaderDeleter
    {
        ReaderAPI::DestroyReaderCGNSFunc destroy = nullptr;

        void operator()(ReaderAPI::ReaderCGNS* reader) const noexcept;
    };

    using ReaderPtr = std::unique_ptr<ReaderAPI::ReaderCGNS, ReaderDeleter>;

    std::unique_ptr<ModuleGuard> m_module_guard;
    std::unique_ptr<ReaderCGNSLogGuard> m_log_guard;
    ReaderPtr m_reader;
};
