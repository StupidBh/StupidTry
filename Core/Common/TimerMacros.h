#pragma once
#include "Logger/logger.hpp"
#include "Utils/ScopedTimer.hpp"

#define UTILS_DETAIL_CONCAT_IMPL(x, y) x##y
#define UTILS_DETAIL_CONCAT(x, y)      UTILS_DETAIL_CONCAT_IMPL(x, y)

#define SCOPED_TIMER(out_msg) decltype(auto) UTILS_DETAIL_CONCAT(timer_, __COUNTER__) = utils::ScopedTimer(std::string_view(out_msg))
#define SCOPED_TIMER_LOG(out_msg)                             \
    decltype(auto) UTILS_DETAIL_CONCAT(timer_, __COUNTER__) = \
        utils::ScopedTimer(std::string_view(out_msg), [](std::string_view msg) { LOG->info(msg); })
