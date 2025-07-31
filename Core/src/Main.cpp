#include "log/logger.hpp"
#include "SingletonData.hpp"

#include "Function.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    CallCmd("ping 127.0.0.1");

    return 0;
}

