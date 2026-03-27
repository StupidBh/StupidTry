#pragma once
#include "Utils.hpp"

#include <span>

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
        using RawVectorType = std::remove_cvref_t<ValueType>;
        using TrueValueType = typename RawVectorType::value_type;

        HighFive::DataSet data_set =
            std::forward<T>(loc).createDataSet<TrueValueType>(name, HighFive::DataSpace::From(input));
        data_set.write(std::forward<ValueType>(input));
        return data_set;
    }

    template<class ElementType, class T>
    requires HDF5Node<T>
    HighFive::DataSet WriteDataSet(T&& loc, const std::string& name, std::span<ElementType> input)
    {
        HighFive::DataSet data_set =
            std::forward<T>(loc).createDataSet<ElementType>(name, HighFive::DataSpace({ input.size() }));
        data_set.write_raw(input.data());
        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet WriteDataSet2(T&& loc, const std::string& name, ValueType&& input)
    {
        using RawVectorType = std::remove_cvref_t<ValueType>;
        using TrueValueType = typename RawVectorType::value_type;

        std::size_t old_size = 0;
        HighFive::DataSet data_set;
        if (!loc.exist(name)) {
            const std::size_t total_size = input.size();
            const std::size_t chunk_size = std::clamp(total_size / 100, std::size_t(1024), std::size_t(1024 * 1024));

            HighFive::DataSetCreateProps props;
            props.add(HighFive::Chunking(std::vector<hsize_t> { chunk_size }));
            H5Pset_fill_time(props.getId(), H5D_FILL_TIME_NEVER);

            HighFive::DataSpace dataspace = HighFive::DataSpace({ 0 }, { HighFive::DataSpace::UNLIMITED });
            data_set = std::forward<T>(loc).createDataSet<TrueValueType>(name, dataspace, props);
        }
        else {
            data_set = std::forward<T>(loc).getDataSet(name);
            auto current_dims = data_set.getSpace().getDimensions();
            old_size = current_dims[0];
        }

        size_t new_size = input.size();
        data_set.resize({ old_size + new_size });
        data_set.select({ old_size }, { new_size }).write(std::forward<ValueType>(input));
        return data_set;
    }

    template<class ValueType, class T>
    requires HDF5Writable<ValueType, T>
    HighFive::DataSet
        WriteDataSet(T&& loc, const std::string& name, ValueType&& input, const HighFive::CompoundType& comp_type)
    {
        HighFive::DataSet data_set =
            std::forward<T>(loc).createDataSet(name, HighFive::DataSpace({ input.size() }), comp_type);
        data_set.write_raw(std::forward<ValueType>(input).data(), comp_type);

        return data_set;
    }

    template<class T>
    requires HDF5Node<T>
    HighFive::Group GetGroup(T&& loc, const std::string& name)
    {
        if (!loc.exist(name)) {
            return std::forward<T>(loc).createGroup(name);
        }
        else {
            return std::forward<T>(loc).getGroup(name);
        }
    }
}
