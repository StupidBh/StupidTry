#include "FluidExtensions.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace {
    constexpr auto VELOCITY_FIELD_X = [] {
        std::array<std::string_view, 5> res { "velocityx", "velocity_0", "velocity[i]", "ux", "velocity_vectorsx" };
        std::ranges::sort(res);
        return res;
    }();
    constexpr auto VELOCITY_FIELD_Y = [] {
        std::array<std::string_view, 5> res { "velocityy", "velocity_1", "velocity[j]", "uy", "velocity_vectorsy" };
        std::ranges::sort(res);
        return res;
    }();
    constexpr auto VELOCITY_FIELD_Z = [] {
        std::array<std::string_view, 5> res { "velocityz", "velocity_2", "velocity[k]", "uz", "velocity_vectorsz" };
        std::ranges::sort(res);
        return res;
    }();

} // namespace

FluidExtensions::FluidExtensions(ReaderFieldData& fields, LogDispatcher& log_dispatcher) noexcept :
    m_fields(fields),
    m_log_dispatcher(log_dispatcher)
{
}

bool FluidExtensions::HasVelocityField()
{
    this->m_velocity = { };
    std::vector<ReaderAPI::Field> field_names;
    if (!this->m_fields.GetAllFieldFunctionName(field_names)) {
        return false;
    }
    std::array<FieldReference, 3> components;
    std::array<std::array<FieldReference, 3>, 2> position_components;
    for (const auto& [var, sub_vars] : field_names) {
        for (const auto& sub_var : sub_vars) {
            std::string normalized_name = sub_var;
            std::ranges::transform(normalized_name, normalized_name.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });

            std::size_t component = 0;
            if (std::ranges::binary_search(VELOCITY_FIELD_X, normalized_name)) {
                component = 0;
            }
            else if (std::ranges::binary_search(VELOCITY_FIELD_Y, normalized_name)) {
                component = 1;
            }
            else if (std::ranges::binary_search(VELOCITY_FIELD_Z, normalized_name)) {
                component = 2;
            }
            else {
                continue;
            }

            const int position = this->m_fields.GetFieldFunctionPosition(var, sub_var);
            if (position < 0 || position > 1) {
                continue;
            }
            const FieldReference reference { .var = var, .sub_var = sub_var };
            if (components[component].sub_var.empty()) {
                components[component] = reference;
            }
            if (position_components[position][component].sub_var.empty()) {
                position_components[position][component] = reference;
                LOG_TRACE("[HasVelocityField] catch velocity component: [{}]-[{}]", var, sub_var);
            }
        }
    }

    const auto complete = [](const auto& candidate) {
        return std::ranges::all_of(candidate, [](const FieldReference& field) { return !field.sub_var.empty(); });
    };
    for (auto& candidate : position_components) {
        if (complete(candidate)) {
            this->m_velocity = std::move(candidate);
            return true;
        }
    }
    if (complete(components)) {
        this->m_velocity = std::move(components);
        return true;
    }
    LOG_WARN("No valid velocity field");
    return false;
}

int FluidExtensions::GetVelocityFieldPosition()
{
    if (!this->HasVelocityField()) {
        return -1;
    }

    int position = -1;
    for (const auto& [var, sub_var] : this->m_velocity) {
        const int location = this->m_fields.GetFieldFunctionPosition(var, sub_var);
        if (location == -1 || (position != -1 && location != position)) {
            LOG_ERROR("Velocity component [{}] has position {}, expected {}.", sub_var, location, position);
            return -1;
        }
        position = location;
    }

    return position;
}

bool FluidExtensions::GetVelocityField(std::vector<std::vector<ReaderAPI::Real>>& data, std::vector<ReaderAPI::Integer>& ids)
{
    if (this->GetVelocityFieldPosition() == -1) {
        return false;
    }

    std::vector<std::vector<ReaderAPI::Real>> loaded_data;
    loaded_data.reserve(this->m_velocity.size());
    std::vector<ReaderAPI::Integer> loaded_ids;
    for (const auto& [var, sub_var] : this->m_velocity) {
        std::vector<ReaderAPI::Integer> component_ids;
        std::vector<ReaderAPI::Real> component_data;
        if (!this->m_fields.GetFieldFunctionIds(var, sub_var, component_ids) || !this->m_fields.GetFieldFunctionData(var, sub_var, component_data)) {
            return false;
        }
        if (component_ids.empty() || component_ids.size() != component_data.size()) {
            LOG_ERROR("Velocity component [{}] has {} IDs and {} values; expected matching non-empty arrays.", sub_var, component_ids.size(), component_data.size());
            return false;
        }
        if (loaded_data.empty()) {
            loaded_ids = std::move(component_ids);
        }
        else if (component_ids != loaded_ids) {
            LOG_ERROR("Velocity component [{}] IDs do not match [{}].", sub_var, this->m_velocity.front().sub_var);
            return false;
        }
        loaded_data.emplace_back(std::move(component_data));
    }

    data.swap(loaded_data);
    ids.swap(loaded_ids);
    return true;
}
