#include "SingletonData.h"
#include "WindowsFunctions.h"

#include <iostream>
#include <system_error>

#include "Logger/logger.hpp"

namespace spdlog::level {
    static void validate(boost::any& value, const std::vector<std::string>& values, level_enum*, int)
    {
        namespace bpo = boost::program_options;

        bpo::validators::check_first_occurrence(value);
        const auto& text = bpo::validators::get_single_string(values);

        if (text != "debug" && text != "info" && text != "warning" && text != "error" && text != "trace") {
            throw bpo::validation_error(bpo::validation_error::invalid_option_value, "log_level", text);
        }

        value = from_str(text);
    }
}

bool SingletonData::ProcessArguments(int argc, char* argv[])
{
    namespace bpo = boost::program_options;

    bpo::options_description desc("Usage: StupidBhh [options]", 150, 10);
    desc.add_options()                                                                                                                                          //
        ("help", "Display this help message")                                                                                                                   //
        ("input_file", bpo::value<std::string>()->required(), "Path to the input file")                                                                         //
        ("workspace_dir", bpo::value<std::string>(), "Directory for working")                                                                                   //
        ("log_dir", bpo::value<std::string>(), "Directory for log file")                                                                                        //
        ("log_level", bpo::value<spdlog::level::level_enum>()->default_value(spdlog::level::info, "info")->value_name("debug|info|warning|error"), "Log level") //
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
    this->m_vm.at("log_level").value() = spdlog::level::level_enum::trace;
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
    normalize_path_option("input_file");

    if (!this->m_vm.contains("workspace_dir")) {
        const auto input_path = std::filesystem::path(this->m_vm["input_file"].as<std::string>());
        auto work_dir = input_path.parent_path();
        if (work_dir.empty()) {
            work_dir = ".";
            work_dir /= input_path.filename().stem();
        }
        work_dir.make_preferred();
        std::cerr << std::format("Miss parameters <workspace_dir>, use <input_file> parent path: {}", work_dir.string()) << std::endl;
        this->m_vm.emplace("workspace_dir", bpo::variable_value(work_dir.string(), true));
    }
    else {
        normalize_path_option("workspace_dir");
    }

    if (!this->m_vm.contains("log_dir")) {
        const auto workspace_dir = std::filesystem::path(this->m_vm["workspace_dir"].as<std::string>());
        auto log_dir = workspace_dir / "logs";
        log_dir.make_preferred();

        std::cerr << std::format("Miss parameters <log_dir>, use <workspace_dir> parent path: {}", log_dir.string()) << std::endl;
        this->m_vm.emplace("log_dir", bpo::variable_value(log_dir.string(), true));
    }
    else {
        normalize_path_option("log_dir");
    }

    dylog::Logger::get_instance().InitLog(this->m_vm["log_dir"].as<std::string>(), "stupid-bhh", this->m_vm["log_level"].as<spdlog::level::level_enum>());

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
    static const std::filesystem::path workspaceDir = this->GetProgramOptions<std::string>("workspace_dir");
    if (!std::filesystem::exists(workspaceDir)) {
        if (std::error_code ec; !std::filesystem::create_directories(workspaceDir, ec)) {
            LOG_ERROR("Create directories [{}] failed: {}", workspaceDir, ec.message());
            exit(1003);
        }
    }
    return workspaceDir;
}
