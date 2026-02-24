#pragma once
#include "Utils.hpp"
#include "highfive/highfive.hpp"

template<utils::VectorType ValueType, class _Ty>
requires std::same_as<std::remove_cvref_t<_Ty>, HighFive::Group>
HighFive::DataSet WriteDataSet(const std::string& name, ValueType&& data, _Ty&& loc)
{
    using RawVectorType = std::remove_cvref_t<ValueType>;
    using TrueValueType = typename RawVectorType::value_type;

    HighFive::DataSet data_set = loc.createDataSet<TrueValueType>(name, HighFive::DataSpace::From(data));
    data_set.write(std::forward<ValueType>(data));
    return data_set;
}
