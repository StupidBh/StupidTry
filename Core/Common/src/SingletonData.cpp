#include "SingletonData.h"

#include <iostream>

#include "log/logger.hpp"

void stupid::SingletonData::ProcessArguments(int argc, char* argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("Usage: [options]", 150, 10);
    desc.add_options()("help,h", "Display this help message")                                         //
        ("inputPath,i", po::value<std::string>(), "Path to the input file")                           //
        ("workDirectory,w", po::value<std::string>()->required(), "Directory for working (required)") //
        ("cpuNum,n", po::value<int>()->default_value(2), "Number of CPU cores to use (default: 2)")   //
        ("DEBUG", po::bool_switch()->default_value(false), "Enable verbose output")                   //
        ;

    std::ostringstream oss;
    oss << desc;

    try {
        po::store(po::command_line_parser(argc, argv).options(desc).run(), this->m_vm);
        if (this->m_vm.contains("help")) {
            std::cerr << oss.str() << std::endl;
        }

        po::notify(this->m_vm);
    }
    catch (const po::error& e) {
        std::cerr << "Parameter error: " << e.what() << std::endl;
        std::cerr << oss.str() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error in ProcessArguments." << std::endl;
        std::cerr << oss.str() << std::endl;
    }

#ifdef _DEBUG
    this->m_vm.at("DEBUG").value() = true;
#endif

    std::string workDirectory = this->m_vm["workDirectory"].as<std::string>();
    dylog::Logger::get_instance().InitLog(workDirectory, stupid::APP_NAME, this->m_vm["DEBUG"].as<bool>());
}

const boost::program_options::variables_map& stupid::SingletonData::get_variables_map() const noexcept
{
    return m_vm;
}

