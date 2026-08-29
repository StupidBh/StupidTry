#include "AnalysisCGNS.h"
#include "ReaderCGNSLogGuard.h"
#include "WindowsFunctions.h"

#include "Logger/logger.hpp"

namespace {
    std::filesystem::path GetReaderLibraryPath()
    {
        const auto executable_directory = GetExecutableDirectory();
        return executable_directory.empty() ? std::filesystem::path(L"ReaderCGNS.dll") : executable_directory / L"ReaderCGNS.dll";
    }

    template<class Function>
    Function ResolveExport(const HMODULE module, const char* export_name)
    {
        const auto function = reinterpret_cast<Function>(GetProcAddress(module, export_name));
        if (function == nullptr) {
            LOG_ERROR("ReaderCGNS.dll does not export {}: {}", export_name, GetLastError());
        }
        return function;
    }
} // namespace

AnalysisCGNS::AnalysisCGNS() :
    AnalysisCGNS(GetReaderLibraryPath())
{
}

AnalysisCGNS::AnalysisCGNS(const std::filesystem::path& library_path) :
    m_module_guard(LoadModuleGuard(library_path))
{
    if (this->m_module_guard == nullptr) {
        return;
    }

    const HMODULE module = this->m_module_guard->GetModule();
    this->m_log_guard = std::make_unique<ReaderCGNSLogGuard>(module);
    if (!*this->m_log_guard) {
        LOG_ERROR("Failed to install the ReaderCGNS log callback.");
        return;
    }

    const auto create = ResolveExport<ReaderAPI::CreateReaderCGNSFunc>(module, "CreateReaderCGNS");
    const auto destroy = ResolveExport<ReaderAPI::DestroyReaderCGNSFunc>(module, "DestroyReaderCGNS");
    if (create == nullptr || destroy == nullptr) {
        return;
    }

    ReaderAPI::ReaderCGNS* reader = create();
    if (reader == nullptr) {
        LOG_ERROR("CreateReaderCGNS returned a null reader.");
        return;
    }
    this->m_reader = ReaderPtr(reader, ReaderDeleter { destroy });
}

AnalysisCGNS::~AnalysisCGNS() noexcept
{
    if (this->m_reader != nullptr && this->m_reader->IsOpen()) {
        this->m_reader->Close();
    }
}

AnalysisCGNS::operator bool() const noexcept
{
    return this->m_module_guard != nullptr && this->m_log_guard != nullptr && static_cast<bool>(*this->m_log_guard) && this->m_reader != nullptr;
}

bool AnalysisCGNS::Analyze(const std::string& cgns_file_path) const
{
    if (!static_cast<bool>(*this)) {
        LOG_ERROR("ReaderCGNS is not initialized.");
        return false;
    }
    if (!this->m_reader->Open(cgns_file_path)) {
        return false;
    }

    LOG_INFO("CGNS metadata: version={:.2f}, solver={}", this->m_reader->GetVersion(), this->m_reader->GetSolverType());
    this->m_reader->info();
    return true;
}

void AnalysisCGNS::ReaderDeleter::operator()(ReaderAPI::ReaderCGNS* reader) const noexcept
{
    if (reader != nullptr && this->destroy != nullptr) {
        this->destroy(reader);
    }
}
