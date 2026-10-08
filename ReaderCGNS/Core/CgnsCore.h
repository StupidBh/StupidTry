#pragma once
#include "ReaderMeshData.h"
#include "FluidExtensions.h"

class CgnsCore final : public ReaderMeshData, public ReaderFieldData {
public:
    CgnsCore();
    ~CgnsCore() override = default;

    std::string GetSolverType() const override;

    ReaderAPI::FluidExtensionsBase* GetFluidExtensions() override;

    void info() const override;

protected:
    void clear_cache_data() noexcept override;

private:
    FluidExtensions m_fluid_extensions;
};
