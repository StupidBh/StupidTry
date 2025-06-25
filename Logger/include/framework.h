#pragma once

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#ifndef STUPID_LIBRARY_EXPORTS
#define STUPID_EXPORT_API __declspec(dllexport)
#else
#define STUPID_EXPORT_API __declspec(dllimport)
#endif // STUPID_LIBRARY_EXPORTS

#include <iostream>
#include <filesystem>
