#include "AnalysisCGNS.h"
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
    const auto create = ResolveExport<ReaderAPI::CreateReaderCGNSFunc>(module, "CreateReaderCGNS");
    const auto destroy = ResolveExport<ReaderAPI::DestroyReaderCGNSFunc>(module, "DestroyReaderCGNS");
    if (create == nullptr || destroy == nullptr) {
        return;
    }

    ReaderAPI::ReaderApiBase* reader = create();
    if (reader == nullptr) {
        LOG_ERROR("CreateReaderCGNS returned a null reader.");
        return;
    }
    this->m_reader = ReaderPtr(reader, ReaderDeleter { destroy });
    if (!this->m_reader->SetLogCallback(LogCallback, nullptr)) {
        LOG_WARN("Failed to install the ReaderCGNS log callback.");
    }
}

AnalysisCGNS::~AnalysisCGNS() noexcept
{
    if (this->m_reader != nullptr) {
        if (this->m_reader->IsOpen()) {
            this->m_reader->Close();
        }
        if (!this->m_reader->ClearLogCallback()) {
            LOG_WARN("Failed to clear the ReaderCGNS log callback.");
        }
        this->m_reader.reset();
    }
}

AnalysisCGNS::operator bool() const noexcept
{
    return this->m_module_guard != nullptr && this->m_reader != nullptr;
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

    this->m_reader->info();

    LOG_INFO("[ReaderCGNS] solver type: {}", this->m_reader->GetSolverType());
    std::vector<std::string> element_set_names;
    if (this->m_reader->GetAllElementSetName(element_set_names)) {
        LOG_INFO("ElementSet: {}", element_set_names);
    }

    return true;
}

void AnalysisCGNS::ReaderDeleter::operator()(ReaderAPI::ReaderApiBase* reader) const noexcept
{
    if (reader != nullptr && this->destroy != nullptr) {
        this->destroy(reader);
    }
}

void AnalysisCGNS::LogCallback(void* context, const ReaderAPI::Logger::LogLevel level, const char* file, const int line, const char* message)
{
    static_cast<void>(context);
    const auto logger = spdlog::default_logger();
    if (logger == nullptr) {
        return;
    }

    spdlog::level::level_enum spd_level;
    switch (level) {
        case ReaderAPI::Logger::READER_CGNS_LOG_TRACE   : spd_level = spdlog::level::trace; break;
        case ReaderAPI::Logger::READER_CGNS_LOG_DEBUG   : spd_level = spdlog::level::debug; break;
        case ReaderAPI::Logger::READER_CGNS_LOG_INFO    : spd_level = spdlog::level::info; break;
        case ReaderAPI::Logger::READER_CGNS_LOG_WARN    : spd_level = spdlog::level::warn; break;
        case ReaderAPI::Logger::READER_CGNS_LOG_ERROR   : spd_level = spdlog::level::err; break;
        case ReaderAPI::Logger::READER_CGNS_LOG_CRITICAL: spd_level = spdlog::level::critical; break;
        default                                         : spd_level = spdlog::level::info; break;
    }

#ifndef NDEBUG
    logger->log(spdlog::source_loc { file, line, "ReaderCGNS" }, spd_level, "[ReaderCGNS] {}", message);
#else
    logger->log(spd_level, "[ReaderCGNS] {}", message);
#endif
}
