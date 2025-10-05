#pragma once
#include "SingletonHolder.hpp"

#include <string>
#include <format>

#include "boost/program_options.hpp"

namespace stupid {
    static constexpr const char* APP_NAME = "stupid-app";

    static constexpr int STUPID_VER_MAJOR = 1;
    static constexpr int STUPID_VER_MINOR = 2;
    static constexpr int STUPID_VER_PATCH = 2;
    static const std::string STUPID_VERSION =
        std::format("{}.{}.{}", STUPID_VER_MAJOR, STUPID_VER_MINOR, STUPID_VER_PATCH);

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
#define CPU_NUM    SINGLE_DATA.VM["cpuNum"].as<int>()
#define IS_DEBUG   SINGLE_DATA.VM["DEBUG"].as<bool>()
