#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>

#include "Utils/SingletonHolder.hpp"
#include "boost/program_options.hpp"
#include "Logger.h"

class SingletonData final : public utils::SingletonHolder<SingletonData> {
    friend class utils::SingletonHolder<SingletonData>;
    SingletonData() = default;

public:
    ~SingletonData() = default;

    bool ProcessArguments(int argc, char* argv[]);
    [[nodiscard]] const std::filesystem::path& GetOrCreateWorkspaceDir();
    [[nodiscard]] const std::filesystem::path& GetOrCreateLogDir();

    [[nodiscard]] Logger& GetLogger() noexcept { return this->m_logger; }

    template<class T>
    T& GetProgramOptions(const std::string& key)
    {
        if (!this->m_vm.contains(key)) {
            throw std::logic_error("No such option \"" + key + "\"");
        }
        return this->m_vm.at(key).as<T>();
    }

private:
    // Keep logging available while all subsequent members are destroyed.
    Logger m_logger;
    boost::program_options::variables_map m_vm;
};

#define SINGLE_DATA SingletonData::get_instance()

#define INPUT_FILE    SINGLE_DATA.GetProgramOptions<std::string>("input_file")
#define WORKSPACE_DIR SINGLE_DATA.GetProgramOptions<std::string>("workspace_dir")

#define LOG_DIR_PATH       SINGLE_DATA.GetOrCreateLogDir()
#define WORKSPACE_DIR_PATH SINGLE_DATA.GetOrCreateWorkspaceDir()
