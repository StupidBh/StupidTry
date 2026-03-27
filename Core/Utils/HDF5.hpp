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
        else if constexpr (std::is_same_v<T, char>) {
            return H5::PredType::NATIVE_CHAR;
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

    template<class T>
    concept HDF5Node = std::derived_from<std::remove_cvref_t<T>, H5::Group>;

    template<class ValueType, class T>
    concept HDF5Writable = (utils::VectorType<ValueType> || utils::ArrayType<ValueType>) && HDF5Node<T>;

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    H5::DataSet WriteDataSet(T&& loc, const std::string& name, ValueType&& input)
    {
        using RawVectorType = std::remove_cvref_t<ValueType>;
        using TrueValueType = typename RawVectorType::value_type;

        hsize_t dims = input.size();
        H5::DataSpace dataspace(1, &dims);
        H5::PredType pred_type = H5_NATIVE_TYPE<TrueValueType>();

        H5::DataSet data_set = std::forward<T>(loc).createDataSet(name, pred_type, dataspace);
        data_set.write(std::forward<ValueType>(input).data(), pred_type);
        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    H5::DataSet WriteDataSet(T&& loc, const std::string& name, ValueType&& input, const H5::CompType& comp_type)
    {
        hsize_t dims = input.size();
        H5::DataSpace dataspace(1, &dims);

        H5::DataSet data_set = std::forward<T>(loc).createDataSet(name, comp_type, dataspace);
        data_set.write(std::forward<ValueType>(input).data(), comp_type);
        return data_set;
    }
}
