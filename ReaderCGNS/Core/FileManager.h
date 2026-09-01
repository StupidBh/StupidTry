#pragma once
#include "ReaderAPI/ReaderApiBase.h"
#include "Logger.h"

#include <map>
#include <unordered_map>

class FileManager : public ReaderAPI::ReaderApiBase {
    struct BaseZone
    {
        int index_base;
        std::vector<int> zone_indices;
    };

    using BaseZoneIndices = std::map<int, BaseZone>;

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
    virtual void clear_cache_data() noexcept;

    int get_file_id() const noexcept;
    [[nodiscard]] std::vector<std::pair<int, std::vector<int>>> get_base_zone_indices() const;
    [[nodiscard]] const BaseZone* get_base_zone_indices(int base) const noexcept;
    [[nodiscard]] const BaseZone* get_base_zone_indices(const std::string& base_name) const noexcept;

    LogDispatcher& GetLogDispatcher() const noexcept;

private:
    void clear_file_data() noexcept;

    bool initialize_base_zone_layout();

    mutable LogDispatcher m_log_dispatcher;

    int m_file_id = 0;
    std::string m_cgns_file_path;

    BaseZoneIndices m_base_zone_indices;
    std::unordered_map<std::string, const BaseZone*> m_base_zone_layout;
};
