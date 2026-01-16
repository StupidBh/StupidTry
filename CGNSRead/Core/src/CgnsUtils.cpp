#include "CgnsUtils.h"

#include "cgns/cgnslib.h"
#include "log/logger.hpp"

int cgns::CG_INFO(int status)
{
    if (status != CG_OK) {
        LOG_ERROR("CGNS Faild: {}", cg_get_error());
        LOG->flush();
    }
    return status;
}
