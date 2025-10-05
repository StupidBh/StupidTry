#pragma once

#ifdef _WIN32
    #define NOMINMAX
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#ifndef STUPID_EXPORT_LIBRARY
    #define LOG_EXPORT_API __declspec(dllexport)
#else
    #define LOG_EXPORT_API __declspec(dllimport)
#endif

#include <iostream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
