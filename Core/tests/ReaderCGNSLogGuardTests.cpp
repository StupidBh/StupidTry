#include "ReaderCGNSLogGuard.h"

#include "ReaderCGNS/ReaderCGNS.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/ostream_sink.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace {
    constexpr std::string_view MissingCgnsPath = "ReaderCGNS-log-guard-test-missing.cgns";
    int failures = 0;

    void Check(const bool condition, const std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++failures;
        }
    }
} // namespace

int main()
{
    Check(ReaderCGNS::Logger::ClearLogCallback(), "initial callback state can be cleared");
    const ReaderCGNSLogGuard null_logger_guard(nullptr);
    Check(!null_logger_guard, "a null logger is rejected");

    std::ostringstream output;
    const auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output, true);
    auto logger = std::make_shared<spdlog::logger>("ReaderCGNSLogGuardTests", sink);
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("%v");

    {
        const ReaderCGNSLogGuard guard(logger.get());
        Check(static_cast<bool>(guard), "the guard installs its callback");

        const std::string output_before_log = output.str();
        ReaderCGNS::info(std::string(MissingCgnsPath));
        const std::string output_during_guard = output.str();
        Check(output_during_guard.size() > output_before_log.size(), "ReaderCGNS messages are forwarded while the guard is active");
        Check(output_during_guard.find("[ReaderCGNS]") != std::string::npos, "forwarded messages include the ReaderCGNS prefix");
        Check(output_during_guard.find(MissingCgnsPath) != std::string::npos, "forwarded messages preserve their content");
    }

    const std::string output_after_clear = output.str();
    ReaderCGNS::info(std::string(MissingCgnsPath));
    Check(output.str() == output_after_clear, "ReaderCGNS messages stop after the guard is destroyed");

    return failures == 0 ? 0 : 1;
}
