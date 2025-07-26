#include "log/logger.hpp"
#include "SingletonData.hpp"

#include "Function.h"

#include "highfive/highfive.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    auto C = ProcessArguments(argc, argv);

    CallCmd("ping 127.0.0.1 -n 3");

    HighFive::File file("./Hdf5.dat", HighFive::File::Overwrite);
    file.createGroup("Test").createAttribute("TestAttribute", 42);

    return 0;
}

