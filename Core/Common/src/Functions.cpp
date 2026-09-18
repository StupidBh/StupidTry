#include "Functions.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "Logger/logger.hpp"
#include "boost/process/ext/exe.hpp"
#include "boost/process/v1/args.hpp"
#include "boost/process/v1/child.hpp"
#include "boost/process/v1/env.hpp"
#include "boost/process/v1/exe.hpp"
#include "boost/process/v1/group.hpp"
#include "boost/process/v1/io.hpp"
#include "boost/process/v2/pid.hpp"

namespace {
    constexpr unsigned char ToAsciiLower(unsigned char value) noexcept
    {
        return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
    }
} // namespace

std::size_t FindCaseInsensitive(std::string_view main_str, std::string_view sub_str) noexcept
{
    if (sub_str.empty()) {
        return 0;
    }

    const auto res = std::ranges::search(main_str, sub_str, [](unsigned char lhs, unsigned char rhs) { return ToAsciiLower(lhs) == ToAsciiLower(rhs); });
    if (res.begin() == main_str.end()) {
        return std::string_view::npos;
    }

    return static_cast<std::size_t>(std::ranges::distance(main_str.begin(), res.begin()));
}

bool IEquals(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    return std::ranges::equal(lhs, rhs, [](unsigned char lhs_char, unsigned char rhs_char) { return ToAsciiLower(lhs_char) == ToAsciiLower(rhs_char); });
}

std::string_view TrimSpaces(const std::string_view str) noexcept
{
    constexpr std::string_view whitespace_chars = " \t\n\r\f\v";
    const std::size_t first = str.find_first_not_of(whitespace_chars);
    if (first == std::string_view::npos) {
        return { };
    }

    const std::size_t last = str.find_last_not_of(whitespace_chars);
    return str.substr(first, last - first + 1);
}

std::string_view StripEdgeChar(std::string_view str, char c) noexcept
{
    if (str.empty()) {
        return str;
    }
    if (str.front() == c) {
        str.remove_prefix(1);
    }
    if (!str.empty() && str.back() == c) {
        str.remove_suffix(1);
    }
    return str;
}

std::string GetEnv(const std::string& env)
{
    if (const char* value = std::getenv(env.c_str())) {
        return value;
    }

    throw std::runtime_error(std::string("Environment variable not found: ") + env);
    return { };
}

std::filesystem::path GetExecutablePath()
{
    boost::system::error_code ec;
    const auto path = boost::process::v2::ext::exe(boost::process::v2::current_pid(), ec);
    if (ec) {
        LOG_ERROR("Boost.Process exe failed: {}", ec.message());
        return { };
    }
    return std::filesystem::path(path.native());
}

std::filesystem::path GetExecutableDirectory()
{
    return GetExecutablePath().parent_path();
}

void ExecuteProcess(const ExecuteProcessConfig& config, std::function<ExecuteProcessAction(const std::string&)> callback)
{
    const auto& exe_path = config.exe_path;
    if (exe_path.empty()) {
        LOG_ERROR("Executable path is empty.");
        return;
    }

    for (const auto& [name, value] : config.environment) {
        if (name.empty() || name.find('=') != std::string::npos || name.find('\0') != std::string::npos || value.find('\0') != std::string::npos) {
            LOG_ERROR("Invalid child environment entry [{}].", name);
            return;
        }
    }

    constexpr auto graceful_exit_timeout = std::chrono::seconds(5);
    namespace process = boost::process::v1;

    try {
        process::group process_group;
        process::ipstream output;
        process::child child;
        if (config.environment.empty()) {
            child = process::child(process::exe(exe_path.native()),
                                   process::args(config.args),
                                   process::std_in<process::null, (process::std_out & process::std_err)> output,
                                   process_group);
        }
        else {
            process::environment child_environment = boost::this_process::environment();
            for (const auto& [name, value] : config.environment) {
                bool updated = false;
                for (auto entry : child_environment) {
#ifdef _WIN32
                    if (!IEquals(entry.get_name(), name)) {
                        continue;
                    }
#else
                    if (entry.get_name() != name) {
                        continue;
                    }
#endif
                    entry.assign(value);
                    updated = true;
                    break;
                }
                if (!updated) {
                    child_environment[name] = value;
                }
            }

            child = process::child(process::exe(exe_path.native()),
                                   process::args(config.args),
                                   process::env(child_environment),
                                   process::std_in<process::null, (process::std_out & process::std_err)> output,
                                   process_group);
        }

        auto action = ExecuteProcessAction::Continue;
        std::string line;
        while (action == ExecuteProcessAction::Continue && std::getline(output, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (callback) {
                action = callback(line);
            }
            else {
                std::cout << line << '\n' << std::flush;
            }
        }

        if (action == ExecuteProcessAction::StopReadingAndWait) {
            output.close();
            const auto deadline = std::chrono::steady_clock::now() + graceful_exit_timeout;
            // ponytail: 10 ms polling avoids deprecated v1 wait_for; use v2 async_wait for event-driven timing.
            bool child_running = child.running();
            while (child_running && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                child_running = child.running();
            }
            if (child_running) {
                process_group.terminate();
            }
            child.wait();
        }
        else if (action == ExecuteProcessAction::TerminateImmediately) {
            output.close();
            process_group.terminate();
            child.wait();
        }
        else {
            child.wait();
        }
    }
    catch (const std::exception& error) {
        LOG_ERROR("Boost.Process failed to execute [{}]: {}", exe_path.string(), error.what());
    }
}
