#include "WindowsFunctions.h"
#include "Functions.h"

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <memory>

#include "Logger/logger.hpp"

ModuleGuard::ModuleGuard(const std::filesystem::path& library_path) noexcept :
    m_module(LoadLibraryW(library_path.c_str()))
{
}

ModuleGuard::~ModuleGuard() noexcept
{
    if (this->m_module != nullptr) {
        FreeLibrary(static_cast<HMODULE>(this->m_module));
    }
}

ModuleGuard::ModuleGuard(ModuleGuard&& other) noexcept :
    m_module(std::exchange(other.m_module, nullptr))
{
}

ModuleGuard& ModuleGuard::operator=(ModuleGuard&& other) noexcept
{
    if (this != &other) {
        if (this->m_module != nullptr) {
            FreeLibrary(static_cast<HMODULE>(this->m_module));
        }
        this->m_module = std::exchange(other.m_module, nullptr);
    }
    return *this;
}

ModuleGuard::ModuleProc ModuleGuard::GetExport(const char* name) const
{
    const auto proc = GetProcAddress(static_cast<HMODULE>(this->m_module), name);
    if (proc == nullptr) {
        const DWORD error = GetLastError();
        LOG_ERROR("GetProcAddress({}) failed: {}", name, error);
    }
    return reinterpret_cast<ModuleProc>(proc);
}

std::unique_ptr<ModuleGuard> LoadModuleGuard(const std::filesystem::path& library_path)
{
    auto module_guard = std::make_unique<ModuleGuard>(library_path);
    if (*module_guard) {
        return module_guard;
    }

    LOG_ERROR("LoadLibraryW [{}] failed: {}", library_path, GetLastError());
    return nullptr;
}
