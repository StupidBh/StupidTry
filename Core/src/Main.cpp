#include "Function.h"
#include "SingletonData.h"

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

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SINGLE_DATA.ProcessArguments(argc, argv);
    if (SINGLE_DATA_VM.empty()) {
        return EXIT_FAILURE;
    }

    for (auto& [key, value] : SINGLE_DATA_VM) {
        if (value.empty()) {
            LOG_WARN("<empty>-[{}] = <empty>", key);
        }
        else if (value.value().type() == typeid(std::string)) {
            LOG_INFO("<string>-[{}] = {}", key, value.as<std::string>());
        }
        else if (value.value().type() == typeid(int)) {
            LOG_INFO("<int>-[{}] = {}", key, value.as<int>());
        }
        else if (value.value().type() == typeid(bool)) {
            LOG_INFO("<bool>-[{}] = {}", key, value.as<bool>());
        }
        else {
            LOG_WARN("[{}] = <unhandled type>", key);
        }
    }

    spdlog::shutdown();
    return 0;
}
