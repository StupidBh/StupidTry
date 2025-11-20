#include "Function.h"
#include "SingletonData.h"

#include "CgnsCore.h"

#include <unordered_set>
#include <ranges>
#include <algorithm>
#include <string>
#include <fstream>

#include <codecvt>
#include <locale>

#include "highfive/highfive.hpp"

template<utils::VectorType ValueType, class _Ty>
HighFive::DataSet WriteDataSet(const std::string& name, ValueType&& data, _Ty&& loc)
{
    using RawVectorType = std::remove_cvref_t<ValueType>;
    using TrueValueType = typename RawVectorType::value_type;

    HighFive::DataSet data_set = loc.createDataSet<TrueValueType>(name, HighFive::DataSpace::From(data));
    data_set.write(std::forward<ValueType>(data));
    return data_set;
}

template<utils::VectorType ValueType, class _Ty>
HighFive::DataSet WriteDataSet(ValueType&& data, _Ty&& loc)
{
    return WriteDataSet<ValueType, _Ty>("Test", std::forward<ValueType>(data), std::forward<_Ty>(loc));
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }
    cgns::InitLog(LOG);

    for (auto& [key, value] : SINGLE_DATA_VM) {
        if (value.empty()) {
            LOG_WARN("Param: {} = <empty>", key);
        }
        else if (value.value().type() == typeid(std::string)) {
            LOG_INFO("Param: {} = {}", key, value.as<std::string>());
        }
        else if (value.value().type() == typeid(int)) {
            LOG_INFO("Param: {} = {}", key, value.as<int>());
        }
        else if (value.value().type() == typeid(bool)) {
            LOG_INFO("Param: {} = {}", key, value.as<bool>());
        }
        else {
            LOG_WARN("Param: {} = <unhandled type>", key);
        }
    }

    spdlog::shutdown();
    return 0;
}
