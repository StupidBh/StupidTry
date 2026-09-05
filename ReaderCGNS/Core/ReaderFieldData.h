#pragma once
#include "CgnsFiled.hpp"
#include "FileManager.h"

class ReaderFieldData : virtual public FileManager {
    struct FieldIndex
    {
        int base;
        int zone;
        int solution;
        int field;
    };

    using FieldIndices = std::vector<FieldIndex>;

public:
    ReaderFieldData() = default;
    ~ReaderFieldData() override = default;

    bool GetAllFieldFunctionName(std::vector<std::string>& field_names) final;

protected:
    void clear_field_data() noexcept;

    bool initialize_field_layout();

private:
    std::unordered_map<std::string, FieldIndices> m_field_layout;
};
