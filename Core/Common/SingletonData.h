#pragma once
#include <filesystem>

#include "Utils/SingletonHolder.hpp"
#include "boost/program_options.hpp"

class SingletonData final : public utils::SingletonHolder<SingletonData> {
    SINGLETON_CLASS(SingletonData);
    SingletonData() = default;

public:
    void ProcessArguments(int argc, char* argv[]);

    const boost::program_options::variables_map& GetProgramOptions() const noexcept;
    const std::filesystem::path& GetWorkDirectory() const;

    template<class T>
    T GetProgramOptions(const std::string& key) const
    {
        if (!this->m_vm.contains(key)) {
            throw std::logic_error("No such option \"" + key + "\"");
        }
        return this->m_vm[key].as<T>();
    }

private:
    boost::program_options::variables_map m_vm;
};

#define SINGLE_DATA    SingletonData::get_instance()
#define SINGLE_DATA_VM SINGLE_DATA.GetProgramOptions()

#define INPUT_PATH SINGLE_DATA.GetProgramOptions<std::string>("inputPath")
#define WORK_DIR   SINGLE_DATA.GetProgramOptions<std::string>("workDirectory")
#define IS_DEBUG   SINGLE_DATA.GetProgramOptions<bool>("DEBUG")

#define WORK_DIR_PATH SINGLE_DATA.GetWorkDirectory()
