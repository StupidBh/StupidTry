#pragma once

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#ifndef STUPID_EXPORT_LIBRARY
    #define STUPID_EXPORT_API __declspec(dllexport)
#else
    #define STUPID_EXPORT_API __declspec(dllimport)
#endif

#include <iostream>
#include <filesystem>
#include <shared_mutex>

class STUPID_EXPORT_API std::shared_mutex;
