#include "Function.h"
#include "SingletonData.h"

#include "CgnsCore.h"

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
    SCOPED_TIMER(std::filesystem::path(argv[0]).filename().string());

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

    LOG_INFO("STUPID_VER_MAJOR: {}", stupid::STUPID_VER_MAJOR);
    LOG_WARN("STUPID_VER_MINOR: {}", stupid::STUPID_VER_MINOR);
    LOG_DEBUG("STUPID_VER_PATCH: {}", stupid::STUPID_VER_PATCH);
    LOG_ERROR("STUPID_VERSION: {}", stupid::STUPID_VERSION);

    return 0;
}
