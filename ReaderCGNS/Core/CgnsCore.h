#pragma once
#include "FileManager.h"

class CgnsCore : virtual public FileManager {
public:
    bool info() override;
    void* QueryInterface() override;
};
