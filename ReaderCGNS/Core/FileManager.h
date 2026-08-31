#pragma once
#include "ReaderAPI/ReaderApiBase.h"
#include "Logger.h"
#include "CgnsTypes.hpp"

#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

class FileManager : public ReaderAPI::ReaderApiBase {
    struct BaseZone
    {
        int base;
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
    int get_file_id() const noexcept;
    [[nodiscard]] std::vector<std::pair<int, std::vector<int>>> get_base_zone_indices() const;

    LogDispatcher& GetLogDispatcher() const noexcept;

private:
    virtual bool initialize_file_data();
    virtual void clear_file_data() noexcept;

    void clear_data();
    bool initialize_base_zone_layout();

    mutable LogDispatcher m_log_dispatcher;

    int m_file_id = 0;
    std::string m_cgns_file_path;

    BaseZoneIndices m_base_zone_indices;
    std::unordered_map<std::string, const BaseZone*> m_base_zone_layout;
};
