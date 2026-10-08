#pragma once
#include "ReaderFieldData.h"

class FluidExtensions final : public ReaderAPI::FluidExtensionsBase {
public:
    explicit FluidExtensions(ReaderFieldData& fields) noexcept :
        m_fields(fields)
    {
    }

    [[nodiscard]] bool HasVelocityField() const override;

private:
    ReaderFieldData& m_fields;
};
