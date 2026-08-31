#pragma once
#include <string>

#include "Utils/Utils.hpp"
#include "highfive/highfive.hpp"

namespace HFUtils {
    // HDF5Node: HighFive 节点类型约束
    template<class T>
    concept HDF5Node = std::derived_from<std::remove_cvref_t<T>, HighFive::NodeTraits<std::remove_cvref_t<T>>>;

    // HDF5Writable: 可写入 DataSet 的容器类型
    template<class ValueType, class T>
    concept HDF5Writable = (utils::VectorType<ValueType> || utils::ArrayType<ValueType>) && HDF5Node<T>;

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet WriteDataSet(T&& loc, const std::string& name, ValueType&& input)
    {
        HighFive::DataSet data_set = std::forward<T>(loc).createDataSet(name, input);
        data_set.write(std::forward<ValueType>(input));
        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet WriteDataSet(T&& loc, const std::string& name, ValueType&& input, const HighFive::CompoundType& comp_type)
    {
        HighFive::DataSet data_set = std::forward<T>(loc).createDataSet(name, HighFive::DataSpace({ input.size() }), comp_type);
        data_set.write_raw(std::forward<ValueType>(input).data(), comp_type);

        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet WriteDataSetAppendable(T&& loc, const std::string& name, ValueType&& input)
    {
        std::size_t old_size = 0;
        HighFive::DataSet data_set;
        if (!loc.exist(name)) {
            const std::size_t total_size = input.size();
            const std::size_t chunk_size = std::clamp(total_size / 100, static_cast<std::size_t>(1024), static_cast<std::size_t>(1024 * 1024));

            HighFive::DataSetCreateProps props;
            props.add(HighFive::Chunking(std::vector<hsize_t> { chunk_size }));
            H5Pset_fill_time(props.getId(), H5D_FILL_TIME_NEVER);

            HighFive::DataSpace dataspace = HighFive::DataSpace({ 0 }, { HighFive::DataSpace::UNLIMITED });
            data_set = std::forward<T>(loc).createDataSet(name, dataspace, props);
        }
        else {
            data_set = std::forward<T>(loc).getDataSet(name);
            const auto current_dims = data_set.getSpace().getDimensions();
            old_size = current_dims[0];
        }

        std::size_t new_size = input.size();
        data_set.resize({ old_size + new_size });
        data_set.select({ old_size }, { new_size }).write(std::forward<ValueType>(input));
        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet WriteDataSetAppendable(T&& loc, const std::string& name, ValueType&& input, const HighFive::CompoundType& comp_type)
    {
        std::size_t old_size = 0;
        HighFive::DataSet data_set;
        if (!loc.exist(name)) {
            const std::size_t total_size = input.size();
            const std::size_t chunk_size = std::clamp(total_size / 100, static_cast<std::size_t>(1024), static_cast<std::size_t>(1024 * 1024));

            HighFive::DataSetCreateProps props;
            props.add(HighFive::Chunking(std::vector<hsize_t> { chunk_size }));

            using ElementType = typename std::remove_cvref_t<ValueType>::value_type;
            ElementType fill_value { };
            if (H5Pset_fill_value(props.getId(), comp_type.getId(), &fill_value) < 0) {
                throw HighFive::PropertyException("Failed to set the compound dataset fill value");
            }

            H5D_fill_value_t fill_status = H5D_FILL_VALUE_ERROR;
            if (H5Pfill_value_defined(props.getId(), &fill_status) < 0 || fill_status == H5D_FILL_VALUE_UNDEFINED) {
                throw HighFive::PropertyException("The compound dataset fill value is undefined");
            }

            HighFive::DataSpace dataspace = HighFive::DataSpace({ 0 }, { HighFive::DataSpace::UNLIMITED });
            data_set = std::forward<T>(loc).createDataSet(name, dataspace, comp_type, props);
        }
        else {
            data_set = std::forward<T>(loc).getDataSet(name);
            const auto current_dims = data_set.getSpace().getDimensions();
            old_size = current_dims[0];
        }

        std::size_t new_size = input.size();
        data_set.resize({ old_size + new_size });
        data_set.select({ old_size }, { new_size }).write_raw(std::forward<ValueType>(input).data(), comp_type);
        return data_set;
    }

    template<class ValueType, class T>
    requires std::derived_from<std::remove_cvref_t<T>, HighFive::DataSet>
    void ReadDataSet(T&& loc, ValueType& input)
    {
        auto dataset_dims = std::forward<T>(loc).getDimensions();
        input.resize(dataset_dims.front());
        std::forward<T>(loc).read(input);
    }

    template<class ValueType, class T>
    requires std::derived_from<std::remove_cvref_t<T>, HighFive::DataSet>
    void ReadDataSet(T&& loc, ValueType& input, const HighFive::CompoundType& comp_type)
    {
        auto dataset_dims = std::forward<T>(loc).getDimensions();
        input.resize(dataset_dims.front());
        std::forward<T>(loc).read_raw(input.data(), comp_type);
    }

    template<class T>
    requires HDF5Node<T>
    HighFive::Group GetOrCreateGroup(T&& loc, const std::string& name)
    {
        if (!loc.exist(name)) {
            return std::forward<T>(loc).createGroup(name);
        }
        else {
            return std::forward<T>(loc).getGroup(name);
        }
    }
} // namespace HFUtils
