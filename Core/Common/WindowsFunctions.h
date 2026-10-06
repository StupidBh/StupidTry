#pragma once
#include <filesystem>
#include <memory>

class ModuleGuard final {
public:
    using ModuleProc = void (*)();

    explicit ModuleGuard(const std::filesystem::path& library_path) noexcept;
    ~ModuleGuard() noexcept;

    ModuleGuard(const ModuleGuard&) = delete;
    ModuleGuard& operator=(const ModuleGuard&) = delete;
    ModuleGuard(ModuleGuard&& other) noexcept;
    ModuleGuard& operator=(ModuleGuard&& other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return this->m_module != nullptr; }

    [[nodiscard]] ModuleProc GetExport(const char* name) const;

private:
    void* m_module = nullptr;
};

[[nodiscard]] std::unique_ptr<ModuleGuard> LoadModuleGuard(const std::filesystem::path& library_path);
