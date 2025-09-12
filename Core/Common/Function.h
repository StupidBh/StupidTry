#pragma once
#include "Utils.hpp"
#include "ScopedTimer.hpp"

#include "system_info.h"

#include "log/logger.hpp"
#include "boost/program_options.hpp"

#define SCOPED_TIMER(out_msg)                    \
    decltype(auto) CONCAT(timer_, __COUNTER__) = \
        utils::ScopedTimer(std::string_view(out_msg), [&](std::string_view msg) { LOG_INFO(msg); })

boost::program_options::variables_map ProcessArguments(int argc, char* argv[]);

bool IsLikelyGBK(std::string_view str);
std::string GBKToUTF8(const std::string& gbk_str);
std::string_view TrimNewline(std::string_view sv);

void CallCmd(const std::string& command, bool open_log = false);

std::string GetEnv(const std::string& env);

std::size_t FindCaseInsensitive(const std::string& main_str, const std::string& sub_str);
bool iequals(std::string_view lhs, std::string_view rhs);
std::string TrimTrailingSpaces(std::string_view sv);
