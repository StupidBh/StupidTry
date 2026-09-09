#pragma once
#include "CgnsFiled.hpp"
#include "FileManager.h"

#include <array>

class ReaderFieldData : virtual public FileManager {
    struct FieldIndex
    {
        int base;
        int zone;
        int solution;
        int field;
        std::string source_name;
        CG_GridLocation_t location;
        ReaderAPI::Integer id_offset;
        cgsize_t value_count;
        int data_dim;
        std::array<cgsize_t, 3> dimensions;
    };

    using FieldIndices = std::vector<FieldIndex>;

public:
    ReaderFieldData() = default;
    ~ReaderFieldData() override = default;

    bool GetAllFieldFunctionName(std::vector<std::string>& field_names) final;
    bool GetFieldFunctionData(const std::string& field_name, ReaderAPI::Field& field_data) final;
    bool GetFieldFunctionData(const std::vector<std::string>& field_names, std::vector<ReaderAPI::Field>& field_data) final;

protected:
    void clear_field_data() noexcept;

    bool initialize_field_layout();

private:
    std::unordered_map<std::string, FieldIndices> m_field_layout;
};
