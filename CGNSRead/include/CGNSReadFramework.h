#pragma once

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#ifndef STUPID_EXPORT_LIBRARY
    #define CGNS_EXPORT_API __declspec(dllexport)
#else
    #define CGNS_EXPORT_API __declspec(dllimport)
#endif

#include <string>
#include <string_view>

#include <filesystem>
