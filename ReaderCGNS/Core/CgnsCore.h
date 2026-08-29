#pragma once
#include "FileManager.h"

class CgnsCore : virtual public FileManager {
public:
    void info() override;
};
