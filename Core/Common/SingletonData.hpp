#pragma once
#include "SingletonHolder.hpp"

#include "boost/program_options.hpp"

namespace stupid {
    inline static constexpr const char* APP_NAME = "stupid-app";
    inline static constexpr const char* APP_VERSION = "0.1.0";

    class SingletonData final : public utils::SingletonHolder<SingletonData> {
        SINGLETON_CLASS(SingletonData);
        SingletonData() = default;

    public:
        boost::program_options::variables_map VM;
    };
}

#define SINGLE_DATA stupid::SingletonData::get_instance()

#define INPUT_PATH std::filesystem::path(SINGLE_DATA.VM["inputPath"].as<std::string>())
#define WORK_DIR   std::filesystem::path(SINGLE_DATA.VM["workDirectory"].as<std::string>())
#define CPU_NUM    SINGLE_DATA.VM["cpuNum"].as<std::uint16_t>()
#define IS_DEBUG   SINGLE_DATA.VM["DEBUG"].as<bool>()
