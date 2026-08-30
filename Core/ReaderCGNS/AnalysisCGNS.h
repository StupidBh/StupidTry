#pragma once
#include "ReaderAPI/ReaderApiBase.h"

#include <filesystem>
#include <memory>
#include <string>

class ModuleGuard;

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

        void operator()(ReaderAPI::ReaderApiBase* reader) const noexcept;
    };

    using ReaderPtr = std::unique_ptr<ReaderAPI::ReaderApiBase, ReaderDeleter>;

    static void LogCallback(void* context, ReaderAPI::Logger::LogLevel level, const char* file, int line, const char* message);

    std::unique_ptr<ModuleGuard> m_module_guard;
    ReaderPtr m_reader;
};
