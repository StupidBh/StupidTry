#pragma once
#include <filesystem>

#include "spdlog/fmt/fmt.h"
#include "spdlog/fmt/ranges.h"

template<>
struct fmt::formatter<std::filesystem::path>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template<class FormatContext>
    auto format(const std::filesystem::path& msg, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", msg.string());
    }
};
