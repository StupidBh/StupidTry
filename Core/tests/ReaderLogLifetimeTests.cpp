#include "AnalysisCGNS.h"
#include "Functions.h"
#include "Logger.h"
#include "SingletonData.h"
#include "spdlog/async.h"
#include "spdlog/sinks/base_sink.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <latch>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>

namespace {
    std::filesystem::path reader_library_directory;

    bool CheckPathFormatting()
    {
        const std::filesystem::path path("D:/test/path with spaces/input.cgns");
        const std::string expected = path.string();
        std::string output;
        fmt::format_to(std::back_inserter(output), "{}", path);
        const std::vector<std::filesystem::path> paths { path, path.filename() };
        if (fmt::format("{}", path) != expected || output != expected || fmt::format("{}", fmt::join(paths, "|")) != expected + "|" + path.filename().string()) {
            std::cerr << "Filesystem paths must support formatting, output iterators, and range joins.\n";
            return false;
        }
        return true;
    }

    bool CheckSourceFileStorage(Logger& logger)
    {
        const std::string expected = "D:/test/plugin/Core/src/ReaderMeshData.cpp";
        const char* stored = nullptr;
        {
            std::string temporary = expected;
            stored = logger.InternSourceFile(temporary.c_str());
            temporary.assign(4096, 'x');
        }
        if (std::string_view(stored) != expected || std::string_view(logger.InternSourceFile(nullptr)) != "" ||
            std::string_view(logger.InternSourceFile("")) != "") {
            std::cerr << "Source paths must own their text and accept empty paths.\n";
            return false;
        }
        for (int i = 0; i < 4096; ++i) {
            const auto path = "D:/test/plugin/source-" + std::to_string(i) + ".cpp";
            static_cast<void>(logger.InternSourceFile(path.c_str()));
        }
        std::atomic<bool> stable { true };
        std::vector<std::jthread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&] {
                for (int lookup = 0; lookup < 2000; ++lookup) {
                    if (logger.InternSourceFile(expected.c_str()) != stored || std::string_view(stored) != expected) {
                        stable.store(false, std::memory_order_relaxed);
                    }
                }
            });
        }
        threads.clear();
        if (!stable.load(std::memory_order_relaxed)) {
            std::cerr << "Source path addresses changed during growth or concurrent lookup.\n";
            return false;
        }
        return true;
    }

    class BlockedSink final : public spdlog::sinks::base_sink<std::mutex> {
    public:
        std::latch entered { 1 };
        std::latch release { 1 };
        std::size_t reader_messages = 0;
        std::size_t invalid_source_paths = 0;
        std::size_t formatted_locations = 0;

    protected:
        void sink_it_(const spdlog::details::log_msg& message) override
        {
            const std::string_view payload(message.payload.data(), message.payload.size());
            if (payload == "gate") {
                this->entered.count_down();
                this->release.wait();
                return;
            }
            if (!payload.starts_with("[ReaderCGNS] ")) {
                return;
            }
            ++this->reader_messages;
#ifndef NDEBUG
            MEMORY_BASIC_INFORMATION memory { };
            if (message.source.empty() || message.source.filename == nullptr || VirtualQuery(message.source.filename, &memory, sizeof(memory)) == 0 ||
                memory.State != MEM_COMMIT || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
                ++this->invalid_source_paths;
                return;
            }
            spdlog::memory_buf_t formatted;
            this->formatter_->format(message, formatted);
            const std::string_view output(formatted.data(), formatted.size());
            if (output.find("FileManager.cpp:") != std::string_view::npos) {
                ++this->formatted_locations;
            }
#else
            if (!message.source.empty()) {
                ++this->invalid_source_paths;
            }
#endif
        }

        void flush_() override { }
    };

    struct ShutdownCheck
    {
        std::shared_ptr<BlockedSink> sink;
        bool initialized = false;
        bool opened = false;
        bool unloaded = false;

        ~ShutdownCheck()
        {
            if (this->sink == nullptr) {
                return;
            }
            if (spdlog::thread_pool() != nullptr) {
                std::cerr << "Application Logger destruction did not release the managed logging thread pool." << std::endl;
                std::_Exit(EXIT_FAILURE);
            }
            if (!this->initialized || this->opened || !this->unloaded || this->sink->reader_messages == 0 || this->sink->invalid_source_paths != 0) {
                std::cerr << "Queued ReaderCGNS logs failed after application shutdown: initialized=" << this->initialized << ", opened=" << this->opened
                          << ", unloaded=" << this->unloaded << ", messages=" << this->sink->reader_messages
                          << ", invalid_source_paths=" << this->sink->invalid_source_paths << std::endl;
                std::_Exit(EXIT_FAILURE);
            }
#ifndef NDEBUG
            if (this->sink->formatted_locations != this->sink->reader_messages) {
                std::cerr << "DLL source locations were not preserved by delayed formatting." << std::endl;
                std::_Exit(EXIT_FAILURE);
            }
#endif
            std::cout << "Reader log lifetime checks passed." << std::endl;
        }
    };

    void QueueLogsBeforeUnload(Logger& application_logger, const std::string& input_file, ShutdownCheck& shutdown_check)
    {
        const auto sink = std::make_shared<BlockedSink>();
        sink->set_pattern("%s:%#|%v");
        const auto logger = std::make_shared<spdlog::async_logger>("reader-log-lifetime", sink, spdlog::thread_pool());
        logger->set_level(spdlog::level::trace);
        application_logger.UpdateLog(logger);
        logger->info("gate");
        sink->entered.wait();

        try {
            AnalysisCGNS analysis;
            shutdown_check.initialized = static_cast<bool>(analysis);
            if (shutdown_check.initialized) {
                shutdown_check.opened = analysis.Analyze(input_file);
            }
        }
        catch (...) {
            sink->release.count_down();
            throw;
        }
        shutdown_check.unloaded = GetModuleHandleW(L"ReaderCGNS.dll") == nullptr;
        shutdown_check.sink = sink;
        sink->release.count_down();
        // Results are checked after the application Logger drains the queue during static destruction.
    }
} // namespace

// Keep executable-directory discovery independent of Boost.Process in this test.
std::filesystem::path GetExecutableDirectory()
{
    return reader_library_directory;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Expected ReaderCGNS directory and a non-CGNS input file.\n";
        return 1;
    }
    try {
        reader_library_directory = argv[1];
        spdlog::init_thread_pool(64, 1);
        // Register this observer before SingletonData so it checks the sink after the application Logger is destroyed.
        static ShutdownCheck shutdown_check;
        auto configured_pool = spdlog::thread_pool();
        auto& application_logger = SINGLE_DATA.GetLogger();
        if (spdlog::thread_pool() != configured_pool) {
            std::cerr << "Application Logger initialization replaced the configured logging thread pool.\n";
            return 1;
        }
        configured_pool.reset();
        if (!CheckPathFormatting() || !CheckSourceFileStorage(application_logger)) {
            return 1;
        }
        QueueLogsBeforeUnload(application_logger, argv[2], shutdown_check);
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Reader log lifetime test failed: " << error.what() << '\n';
        return 1;
    }
}
