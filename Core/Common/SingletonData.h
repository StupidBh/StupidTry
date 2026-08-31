#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>

#include "Utils/SingletonHolder.hpp"
#include "boost/program_options.hpp"

class SingletonData final : public utils::SingletonHolder<SingletonData> {
    friend class utils::SingletonHolder<SingletonData>;
    SingletonData() = default;

public:
    ~SingletonData() = default;

    bool ProcessArguments(int argc, char* argv[]);
    [[nodiscard]] const std::filesystem::path& GetOrCreateWorkDirectory();

    template<class T>
    T& GetProgramOptions(const std::string& key)
    {
        if (!this->m_vm.contains(key)) {
            throw std::logic_error("No such option \"" + key + "\"");
        }
        return this->m_vm.at(key).as<T>();
    }

private:
    boost::program_options::variables_map m_vm;
};

#define SINGLE_DATA SingletonData::get_instance()

#define INPUT_PATH SINGLE_DATA.GetProgramOptions<std::string>("inputPath")
#define WORK_DIR   SINGLE_DATA.GetProgramOptions<std::string>("workDirectory")
#define IS_DEBUG   SINGLE_DATA.GetProgramOptions<bool>("DEBUG")

#define WORK_DIR_PATH SINGLE_DATA.GetOrCreateWorkDirectory()
