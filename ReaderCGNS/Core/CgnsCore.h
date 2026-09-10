#pragma once
#include "ReaderMeshData.h"
#include "ReaderFieldData.h"

class CgnsCore final : public ReaderMeshData, public ReaderFieldData {
public:
    CgnsCore() = default;
    ~CgnsCore() override = default;

    std::string GetSolverType() const override;

    void info() const override;

protected:
    void clear_cache_data() noexcept override;
};
