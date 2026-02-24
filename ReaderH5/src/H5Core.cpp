#include "H5Core.h"
#include "hdf5/H5Cpp.h"

void stupid_h5::InitLog(std::shared_ptr<spdlog::logger> log)
{
    dylog::Logger::get_instance().UpdateLog(log);
}
