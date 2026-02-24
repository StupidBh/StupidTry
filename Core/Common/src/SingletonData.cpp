#include "SingletonData.h"

#include <iostream>

#include "log/logger.hpp"

void stupid::SingletonData::ProcessArguments(int argc, char* argv[])
{
    namespace bpo = boost::program_options;

    bpo::options_description desc("Usage: [options]", 150, 10);
    desc.add_options()("help,h", "Display this help message")                                       //
        ("inputPath,i", bpo::value<std::string>()->required(), "Path to the input file")            //
        ("workDirectory,w", bpo::value<std::string>()->default_value("."), "Directory for working") //
        ("DEBUG", bpo::bool_switch()->default_value(false), "Enable verbose output")                //
        ;
    std::ostringstream oss;
    oss << desc;

    try {
        bpo::store(bpo::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), this->m_vm);
        if (this->m_vm.contains("help")) {
            std::cerr << oss.str() << std::endl;
            return;
        }

        bpo::notify(this->m_vm);
    }
    catch (const bpo::error& e) {
        std::cerr << "Parameter error: " << e.what() << std::endl;
        std::cerr << oss.str() << std::endl;
        this->m_vm.clear();
        return;
    }
    catch (...) {
        std::cerr << "Unknown error in ProcessArguments." << std::endl;
        std::cerr << oss.str() << std::endl;
        this->m_vm.clear();
        return;
    }

#ifdef _DEBUG
    this->m_vm.at("DEBUG").value() = true;
#endif

    dylog::Logger::get_instance().InitLog(
        this->m_vm["workDirectory"].as<std::string>(),
        stupid::APP_NAME,
        this->m_vm["DEBUG"].as<bool>());

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
}

const boost::program_options::variables_map& stupid::SingletonData::get_variables_map() const noexcept
{
    return m_vm;
}

