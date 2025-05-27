// pch.h: 这是预编译标头文件。
// 下方列出的文件仅编译一次，提高了将来生成的生成性能。
// 这还将影响 IntelliSense 性能，包括代码完成和许多代码浏览功能。
// 但是，如果此处列出的文件中的任何一个在生成之间有更新，它们全部都将被重新编译。
// 请勿在此处添加要频繁更新的文件，这将使得性能优势无效。

#ifndef PCH_H
#define PCH_H

// 添加要在此处预编译的标头
#include "framework.h"

#include <iostream>
#include <filesystem>
#include <mutex>
#include <shared_mutex>

#include "spdlog/async.h"
#include "spdlog/spdlog.h"
#include "spdlog/stopwatch.h"
#include "spdlog/fmt/ranges.h" // 启用容器格式化支持
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

template class EXPORT_API std::shared_ptr<spdlog::async_logger>;
class EXPORT_API std::shared_mutex;

#endif // PCH_H
