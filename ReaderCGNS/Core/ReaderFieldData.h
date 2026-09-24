#pragma once
#include "CgnsFiled.hpp"
#include "FileManager.h"

class ReaderFieldData : virtual public FileManager {
    using FieldIndices = std::vector<FieldIndex>;

public:
    ReaderFieldData() = default;
    ~ReaderFieldData() override = default;

    bool GetAllFieldFunctionName(std::vector<ReaderAPI::Field>& field_names) final;

protected:
    void clear_field_data() noexcept;

    bool initialize_field_layout();

private:
    BaseZoneOffset m_offset;
    SolutionLocation m_solution_location;
    std::unordered_map<std::string, FieldIndices> m_field_layout;
};
