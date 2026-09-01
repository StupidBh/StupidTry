#pragma once
#include "FileManager.h"

class ReaderFieldData : virtual public FileManager {
public:
    ReaderFieldData() = default;
    ~ReaderFieldData() override = default;

protected:
    void clear_field_data() noexcept;
};
