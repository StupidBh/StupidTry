#include "AnalysisCGNS.h"
#include "Functions.h"
#include "WindowsFunctions.h"

#include <algorithm>
#include <unordered_set>

#include "Logger/logger.hpp"

namespace {
    std::filesystem::path GetReaderLibraryPath()
    {
        const auto executable_directory = GetExecutableDirectory();
        return executable_directory.empty() ? std::filesystem::path(L"ReaderCGNS.dll") : executable_directory / L"ReaderCGNS.dll";
    }

    template<class Function>
    Function ResolveExport(const ModuleGuard& module, const char* export_name)
    {
        return reinterpret_cast<Function>(module.GetExport(export_name));
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

    const auto create = ResolveExport<ReaderAPI::CreateReaderCGNSFunc>(*this->m_module_guard, "CreateReaderCGNS");
    const auto destroy = ResolveExport<ReaderAPI::DestroyReaderCGNSFunc>(*this->m_module_guard, "DestroyReaderCGNS");
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

    // this->m_reader->info();

    LOG_INFO("[ReaderCGNS] solver type: {}", this->m_reader->GetSolverType());

    std::vector<ReaderAPI::Node> all_nodes;
    if (this->m_reader->GetAllNodeCoordinates(all_nodes)) {
        std::unordered_set<int> unique_id;
        for (const auto& [id, x, y, z] : all_nodes) {
            if (unique_id.contains(id)) {
                LOG_WARN("Repeat node: id={}, xyz=[{},{},{}]", id, x, y, z);
                break;
            }
            unique_id.insert(id);
        }
    }

    std::vector<ReaderAPI::Elem> all_elements;
    if (this->m_reader->GetAllElement(all_elements)) {
        std::unordered_set<int> unique_id;
        for (const auto& element : all_elements) {
            if (unique_id.contains(element.id)) {
                LOG_WARN("Repeat elem: id={}, type={}, nodes={}", element.id, element.type, element.nodes);
                break;
            }
            if (std::ranges::any_of(element.nodes, [limit = all_nodes.size()](auto value) { return value < 0 || value >= limit; })) {
                LOG_WARN("Invalid elem in nodes: id={}, type={}, nodes={}", element.id, element.type, element.nodes);
                break;
            }
            unique_id.insert(element.id);
        }
    }

    std::vector<std::string> all_component_names;
    if (this->m_reader->GetAllComponentName(all_component_names)) {
        std::vector<ReaderAPI::Integer> ids;
        for (const auto& component_name : all_component_names) {
            if (this->m_reader->GetComponent(component_name, ids)) {
                if (std::ranges::any_of(ids, [limit = all_elements.size()](auto value) { return value < 0 || value >= limit; })) {
                    LOG_WARN("Invalid component: name={}, ids_range=[{}, {}]", component_name, std::ranges::min(ids), std::ranges::max(ids));
                }
            }
        }
    }

    std::vector<ReaderAPI::Field> field_function_names;
    if (this->m_reader->GetAllFieldFunctionName(field_function_names)) {
        std::vector<ReaderAPI::Field> loaded_fields;
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
