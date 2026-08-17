#include "ReaderCGNSLogGuard.h"

#include "ReaderCGNS/ReaderCGNS.h"

ReaderCGNSLogGuard::~ReaderCGNSLogGuard()
{
    ReaderCGNS::Logger::ClearLogCallback();
}
