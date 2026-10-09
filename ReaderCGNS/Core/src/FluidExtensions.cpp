#include "FluidExtensions.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace {
    constexpr auto VELOCITY_NAME = "velocity";
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
    m_log_dispatcher(log_dispatcher),
    m_velocity(VELOCITY_NAME)
{
}

bool FluidExtensions::HasVelocityField()
{
    this->m_velocity.sub_vars.clear();
    std::vector<ReaderAPI::Field> field_names;
    if (!this->m_fields.GetAllFieldFunctionName(field_names)) {
        return false;
    }
    std::array<std::string, 3> components;

    bool x_flag = true, y_flag = true, z_flag = true;
    for (const auto& [_, sub_vars] : field_names) {
        for (const auto& sub_var : sub_vars) {
            std::string temp_sub_var_name = sub_var;
            std::ranges::transform(temp_sub_var_name, temp_sub_var_name.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });

            if (x_flag && std::ranges::binary_search(VELOCITY_FIELD_X, temp_sub_var_name)) {
                LOG_TRACE("[HasVelocityField] catch velocity-x: [{}]", sub_var);
                x_flag = false;
                components[0] = sub_var;
            }
            else if (y_flag && std::ranges::binary_search(VELOCITY_FIELD_Y, temp_sub_var_name)) {
                LOG_TRACE("[HasVelocityField] catch velocity-y: [{}]", sub_var);
                y_flag = false;
                components[1] = sub_var;
            }
            else if (z_flag && std::ranges::binary_search(VELOCITY_FIELD_Z, temp_sub_var_name)) {
                LOG_TRACE("[HasVelocityField] catch velocity-z: [{}]", sub_var);
                z_flag = false;
                components[2] = sub_var;
            }

            if (!x_flag && !y_flag && !z_flag) {
                break;
            }
        }

        if (!x_flag && !y_flag && !z_flag) {
            LOG_TRACE("[HasVelocityField] catch velocity field finish.");
            break;
        }
    }

    if (x_flag || y_flag || z_flag) {
        LOG_WARN("No valid velocity field");
        return false;
    }
    this->m_velocity.sub_vars.assign(components.begin(), components.end());
    return true;
}

int FluidExtensions::GetVelocityFieldPosition()
{
    if (!this->HasVelocityField()) {
        return -1;
    }

    int position = -1;
    for (const auto& sub_var : this->m_velocity.sub_vars) {
        const int location = this->m_fields.GetFieldFunctionPosition(this->m_velocity.var, sub_var);
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
    loaded_data.reserve(this->m_velocity.sub_vars.size());
    std::vector<ReaderAPI::Integer> loaded_ids;
    const auto& var = this->m_velocity.var;
    for (const auto& sub_var : this->m_velocity.sub_vars) {
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
            LOG_ERROR("Velocity component [{}] IDs do not match [{}].", sub_var, this->m_velocity.sub_vars.front());
            return false;
        }
        loaded_data.emplace_back(std::move(component_data));
    }

    data.swap(loaded_data);
    ids.swap(loaded_ids);
    return true;
}
