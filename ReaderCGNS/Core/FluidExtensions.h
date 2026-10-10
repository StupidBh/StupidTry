#pragma once
#include "ReaderFieldData.h"

class FluidExtensions final : public ReaderAPI::FluidExtensionsBase {
public:
    explicit FluidExtensions(ReaderFieldData& fields, LogDispatcher& log_dispatcher) noexcept;

    [[nodiscard]] bool HasVelocityField() override;
    [[nodiscard]] int GetVelocityFieldPosition() override;
    [[nodiscard]] bool GetVelocityField(std::vector<std::vector<ReaderAPI::Real>>& data, std::vector<ReaderAPI::Integer>& ids) override;

private:
    [[nodiscard]] LogDispatcher& GetLogDispatcher() const noexcept { return this->m_log_dispatcher; }

    ReaderFieldData& m_fields;
    LogDispatcher& m_log_dispatcher;

    struct FieldReference
    {
        std::string var;
        std::string sub_var;
    };

    std::array<FieldReference, 3> m_velocity;
};
