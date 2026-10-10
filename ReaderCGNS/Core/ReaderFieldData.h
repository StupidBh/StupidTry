#pragma once
#include "CgnsFiled.hpp"
#include "FileManager.h"

class ReaderFieldData : virtual public FileManager {
    using FieldIndices = std::vector<FieldIndex>;

public:
    ReaderFieldData() = default;
    ~ReaderFieldData() override = default;

    bool GetAllFieldFunctionName(std::vector<ReaderAPI::Field>& field_names) final;
    int GetFieldFunctionPosition(const std::string& var, const std::string& sub_var) final;
    bool GetFieldFunctionIds(const std::string& var, const std::string& sub_var, std::vector<ReaderAPI::Integer>& ids) final;
    bool GetFieldFunctionData(const std::string& var, const std::string& sub_var, std::vector<ReaderAPI::Real>& data) final;

protected:
    void clear_field_data() noexcept;

    bool initialize_field_layout();

private:
    struct FieldGroup
    {
        int position = -1;
        std::unordered_map<std::string, FieldIndices> fields;
    };

    const FieldIndices* find_field_indices(const std::string& var, const std::string& sub_var);

    BaseZoneOffset m_offset;
    std::unordered_map<std::string, FieldGroup> m_field_groups;
    bool m_field_layout_initialized = false;
};
