#include "FluidExtensions.h"

#include <algorithm>
#include <array>
#include <string_view>

bool FluidExtensions::HasVelocityField() const
{
    std::vector<ReaderAPI::Field> field_names;
    if (!this->m_fields.GetAllFieldFunctionName(field_names)) {
        return false;
    }

    static constexpr std::array<std::string_view, 3> VELOCITY_FIELDS = { "VelocityX", "VelocityY", "VelocityZ" };
    return std::ranges::all_of(VELOCITY_FIELDS, [&field_names](std::string_view required_name) {
        return std::ranges::any_of(field_names, [required_name](const ReaderAPI::Field& field) { return std::ranges::contains(field.sub_vars, required_name); });
    });
}
