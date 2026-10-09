#pragma once
#include "ReaderFieldData.h"

class FluidExtensions final : public ReaderAPI::FluidExtensionsBase {
public:
    explicit FluidExtensions(ReaderFieldData& fields, LogDispatcher& log_dispatcher) noexcept;

    [[nodiscard]] bool HasVelocityField() override;
    [[nodiscard]] int GetVelocityFieldPosition() override;
    [[nodiscard]] bool GetVelocityField(std::vector<std::vector<ReaderAPI::Real>>& data, std::vector<ReaderAPI::Integer>& ids) override;

private:
    LogDispatcher& GetLogDispatcher() const noexcept { return this->m_log_dispatcher; }

    ReaderFieldData& m_fields;
    LogDispatcher& m_log_dispatcher;
    ReaderAPI::Field m_velocity;
};
