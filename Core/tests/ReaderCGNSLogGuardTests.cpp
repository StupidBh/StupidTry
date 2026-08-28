#include "ReaderCGNSLogGuard.h"

#include "ReaderCGNS/ReaderCGNS.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace {
    using ClearLogCallbackFunc = bool (*)() noexcept;

    constexpr std::string_view MissingCgnsPath = "ReaderCGNS-log-guard-test-missing.cgns";
    int failures = 0;

    struct ModuleGuard
    {
        HMODULE module = LoadLibraryW(L"ReaderCGNS.dll");

        ~ModuleGuard()
        {
            if (module != nullptr) {
                FreeLibrary(module);
            }
        }
    };

    void Check(const bool condition, const std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++failures;
        }
    }

    template<class Function>
    Function LoadExport(const HMODULE module, const char* name)
    {
        return reinterpret_cast<Function>(GetProcAddress(module, name));
    }

    void InspectMissingFile(const ReaderAPI::CreateReaderCGNSFunc create, const ReaderAPI::DestroyReaderCGNSFunc destroy)
    {
        std::unique_ptr<ReaderAPI::ReaderCGNS, ReaderAPI::DestroyReaderCGNSFunc> reader(create(), destroy);
        Check(reader != nullptr, "the reader factory creates an instance");
        if (reader != nullptr) {
            reader->Open(std::string(MissingCgnsPath));
        }
    }
} // namespace

int main()
{
    const ModuleGuard module;
    Check(module.module != nullptr, "ReaderCGNS.dll can be loaded");
    if (module.module == nullptr) {
        return 1;
    }

    const auto clear_log_callback = LoadExport<ClearLogCallbackFunc>(module.module, "ClearLogCallback");
    const auto create = LoadExport<ReaderAPI::CreateReaderCGNSFunc>(module.module, "CreateReaderCGNS");
    const auto destroy = LoadExport<ReaderAPI::DestroyReaderCGNSFunc>(module.module, "DestroyReaderCGNS");
    Check(clear_log_callback != nullptr, "the clear-log export can be resolved");
    Check(create != nullptr, "the reader factory export can be resolved");
    Check(destroy != nullptr, "the reader destroy export can be resolved");
    if (clear_log_callback == nullptr || create == nullptr || destroy == nullptr) {
        return 1;
    }

    Check(clear_log_callback(), "initial callback state can be cleared");

    std::ostringstream output;
    const auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output, true);
    auto logger = std::make_shared<spdlog::logger>("ReaderCGNSLogGuardTests", sink);
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("%v");

    const auto previous_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);
    {
        const ReaderCGNSLogGuard guard(module.module);
        Check(static_cast<bool>(guard), "the guard installs its callback");

        const std::string output_before_log = output.str();
        InspectMissingFile(create, destroy);
        const std::string output_during_guard = output.str();
        Check(output_during_guard.size() > output_before_log.size(), "ReaderCGNS messages are forwarded while the guard is active");
        Check(output_during_guard.find("[ReaderCGNS]") != std::string::npos, "forwarded messages include the ReaderCGNS prefix");
        Check(output_during_guard.find(MissingCgnsPath) != std::string::npos, "forwarded messages preserve their content");
    }
    spdlog::set_default_logger(previous_logger);

    const std::string output_after_clear = output.str();
    InspectMissingFile(create, destroy);
    Check(output.str() == output_after_clear, "ReaderCGNS messages stop after the guard is destroyed");

    return failures == 0 ? 0 : 1;
}
