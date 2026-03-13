#pragma once
#include "Utils.hpp"

#include "highfive/highfive.hpp"

template<utils::VectorType ValueType, class T>
requires std::derived_from<std::remove_cvref_t<T>, HighFive::NodeTraits<std::remove_cvref_t<T>>>
HighFive::DataSet WriteDataSet(const std::string& name, ValueType&& input, T&& loc)
{
    using RawVectorType = std::remove_cvref_t<ValueType>;
    using TrueValueType = typename RawVectorType::value_type;

    HighFive::DataSet data_set;

#if (HIGHFIVE_VERSION_MAJOR > 3) || (HIGHFIVE_VERSION_MAJOR == 3 && HIGHFIVE_VERSION_MINOR >= 3)

    data_set = std::forward<T>(loc).createDataSet<TrueValueType>(name, HighFive::DataSpace::From(input));
    data_set.write(std::forward<ValueType>(input));

#elif (HIGHFIVE_VERSION_MAJOR > 2) || (HIGHFIVE_VERSION_MAJOR == 2 && HIGHFIVE_VERSION_MINOR >= 10)

    const std::size_t total_size = input.size();
    const std::size_t chunk_size = std::clamp(total_size / 100, std::size_t(1024), std::size_t(1024 * 1024));

    HighFive::DataSetCreateProps props;
    props.add(HighFive::Chunking(std::vector<hsize_t> { chunk_size }));
    H5Pset_fill_time(props.getId(), H5D_FILL_TIME_NEVER);

    data_set = std::forward<_Ty>(loc).createDataSet<TrueValueType>(name, HighFive::DataSpace::From(input), props);
    for (std::size_t offset = 0; offset < total_size; offset += chunk_size) {
        std::size_t count = std::min(chunk_size, total_size - offset);
        data_set.select({ offset }, { count }).write_raw(input.data() + offset);
    }

#else
    #error "HighFive version is too old!"
#endif
    return data_set;
}
