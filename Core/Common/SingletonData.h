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
        void ProcessArguments(int argc, char* argv[]);
        const boost::program_options::variables_map& get_variables_map() const noexcept;

    private:
        boost::program_options::variables_map m_vm;
    };
}

#define SINGLE_DATA    stupid::SingletonData::get_instance()
#define SINGLE_DATA_VM SINGLE_DATA.get_variables_map()

#define INPUT_PATH SINGLE_DATA_VM["inputPath"].as<std::string>()
#define WORK_DIR   SINGLE_DATA_VM["workDirectory"].as<std::string>()
