#include "SingletonData.h"
#include "WindowsFunctions.h"

#include <iostream>
#include <system_error>

#include "Logger/logger.hpp"

bool SingletonData::ProcessArguments(int argc, char* argv[])
{
    namespace bpo = boost::program_options;

    bpo::options_description desc("Usage: StupidBhh [options]", 150, 10);
    desc.add_options()("help,h", "Display this help message")                            //
        ("inputPath,i", bpo::value<std::string>()->required(), "Path to the input file") //
        ("workDirectory,w", bpo::value<std::string>(), "Directory for working")          //
        ("DEBUG", bpo::bool_switch()->default_value(false), "Enable verbose output")     //
        ;
    std::ostringstream oss;
    oss << desc;

    try {
        bpo::store(bpo::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), this->m_vm);
        if (this->m_vm.contains("help")) {
            std::cerr << oss.str() << std::endl;
            this->m_vm.clear();
            return false;
        }

        bpo::notify(this->m_vm);
    }
    catch (const bpo::error& e) {
        std::cerr << "Parameter error: " << e.what() << std::endl;
        std::cerr << oss.str() << std::endl;
        this->m_vm.clear();
        return false;
    }
    catch (...) {
        std::cerr << "Unknown error in ProcessArguments." << std::endl;
        std::cerr << oss.str() << std::endl;
        this->m_vm.clear();
        return false;
    }

#ifndef NDEBUG
    this->m_vm.at("DEBUG").value() = true;
#endif

    auto normalize_path_option = [this](const std::string& key) {
        const auto iter = this->m_vm.find(key);
        if (iter == this->m_vm.end() || iter->second.empty()) {
            return;
        }
        auto path = std::filesystem::path(iter->second.as<std::string>());
        path.make_preferred();
        iter->second.value() = path.string();
    };
    normalize_path_option("inputPath");

    if (!this->m_vm.contains("workDirectory")) {
        const auto input_path = std::filesystem::path(this->m_vm["inputPath"].as<std::string>());
        auto work_dir = input_path.parent_path();
        if (work_dir.empty()) {
            work_dir = ".";
            work_dir /= input_path.filename().stem();
        }
        work_dir.make_preferred();
        std::cerr << std::format("Miss parameters <workDirectory>, use <inputPath> parent path: {}", work_dir.string()) << std::endl;
        this->m_vm.emplace("workDirectory", bpo::variable_value(work_dir.string(), true));
    }
    else {
        normalize_path_option("workDirectory");
    }

    dylog::Logger::get_instance().InitLog(this->m_vm["workDirectory"].as<std::string>(), "stupid-bhh", this->m_vm["DEBUG"].as<bool>());

#ifndef NDEBUG
    LOG_INFO(GetExecutableDirectory());
    LOG_INFO(GetExecutablePath());
    for (auto& [key, value] : this->m_vm) {
        if (value.empty()) {
            LOG_WARN("<empty>-[{}] = <empty>", key);
        }
        else if (value.value().type() == typeid(std::string)) {
            LOG_DEBUG("<string>-[{}] = {}", key, value.as<std::string>());
        }
        else if (value.value().type() == typeid(int)) {
            LOG_DEBUG("<int>-[{}] = {}", key, value.as<int>());
        }
        else if (value.value().type() == typeid(bool)) {
            LOG_DEBUG("<bool>-[{}] = {}", key, value.as<bool>());
        }
        else {
            LOG_WARN("[{}] = <unhandled type>", key);
        }
    }
#endif
    return true;
}

const std::filesystem::path& SingletonData::GetOrCreateWorkDirectory()
{
    static const std::filesystem::path workDirectory = this->GetProgramOptions<std::string>("workDirectory");
    if (!std::filesystem::exists(workDirectory)) {
        if (std::error_code ec; !std::filesystem::create_directories(workDirectory, ec)) {
            LOG_ERROR("create_directories [{}] failed: {}", workDirectory, ec.message());
            exit(1003);
        }
    }
    return workDirectory;
}
