#pragma once
#include "Utils.hpp"
#include "ScopedTimer.hpp"

#include "system_info.h"

#include "boost/program_options.hpp"

boost::program_options::variables_map ProcessArguments(int argc, char* argv[]);

bool IsLikelyGBK(const std::string& str);
std::string GBKToUTF8(const std::string& gbk_str);

void CallCmd(const std::string& command);

std::string GetEnv(const std::string& env);

std::size_t FindCaseInsensitive(const std::string& main_str, const std::string& sub_str);
