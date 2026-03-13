#pragma once
#include "Utils.hpp"

#include <string_view>
#include <tuple>
#include <utility>

#include "hdf5/H5Cpp.h"

namespace HDF5Utils {
    template<class>
    inline constexpr bool always_false = false;

    template<class T>
    constexpr H5::PredType H5_NATIVE_TYPE()
    {
        if constexpr (std::is_same_v<T, float>) {
            return H5::PredType::NATIVE_FLOAT;
        }
        else if constexpr (std::is_same_v<T, double>) {
            return H5::PredType::NATIVE_DOUBLE;
        }
        else if constexpr (std::is_same_v<T, int>) {
            return H5::PredType::NATIVE_INT;
        }
        else if constexpr (std::is_same_v<T, std::int16_t>) {
            return H5::PredType::NATIVE_INT16;
        }
        else if constexpr (std::is_same_v<T, std::int32_t>) {
            return H5::PredType::NATIVE_INT32;
        }
        else if constexpr (std::is_same_v<T, std::uint16_t>) {
            return H5::PredType::NATIVE_UINT16;
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>) {
            return H5::PredType::NATIVE_UINT32;
        }
        else {
            static_assert(always_false<T>, "Unsupported type for HDF5");
        }
    }

    inline std::vector<std::string> GetDataSetMemberName(H5::DataSet dataset)
    {
        H5::CompType comp_type = dataset.getCompType();
        int member_length = comp_type.getNmembers();

        std::vector<std::string> result(member_length);
        for (int i = 0; i < member_length; ++i) {
            std::string name = comp_type.getMemberName(i);
            result[i] = name;
        }
        return result;
    }

    template<class T, std::size_t N>
    requires std::integral<T> || std::floating_point<T>
    class CompTypeMatrix final {
    public
        static_assert(N > 0, "The matrix dimension must be greater than 0.");
        std::array<T, N> data;

        template<typename Container>
        requires requires(Container c) {
            { *std::begin(c) } -> std::convertible_to<std::string>;
        }
        static H5::CompType CompType(const Container& names)
        {
            constexpr size_t base_offset = offsetof(CompTypeMatrix, data);

            H5::CompType type(sizeof(CompTypeMatrix));
            for (std::size_t i = 0; i < names.size() && i < N; ++i) {
                type.insertMember(names[i], base_offset + sizeof(T) * i, H5_NATIVE_TYPE<T>());
            }

            return type;
        }
    };
}
