#include "ReaderCGNS/ReaderCGNS.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
    using ReaderCGNS::Logger::ReaderCGNS_LogLevel;

    constexpr std::string_view MissingCgnsPath = "ReaderCGNS-logger-callback-test-missing.cgns";
    int failures = 0;

    void Check(const bool condition, const std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++failures;
        }
    }

    struct LogRecord
    {
        ReaderCGNS_LogLevel level;
        std::string file;
        int line;
        std::string message;
    };

    struct CaptureContext
    {
        std::mutex mutex;
        std::vector<LogRecord> records;
    };

    void CaptureCallback(void* context, const ReaderCGNS_LogLevel level, const char* file, const int line, const char* message)
    {
        auto& capture = *static_cast<CaptureContext*>(context);
        const std::scoped_lock lock(capture.mutex);
        capture.records.push_back(LogRecord { level, file, line, message });
    }

    void ThrowingCallback(void* context, ReaderCGNS_LogLevel, const char*, int, const char*)
    {
        static_cast<std::atomic_size_t*>(context)->fetch_add(1, std::memory_order_relaxed);
        throw std::runtime_error("callback failure");
    }

    struct BlockingContext
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool entered = false;
        bool release = false;
        std::atomic_size_t calls { 0 };
    };

    void BlockingCallback(void* context, ReaderCGNS_LogLevel, const char*, int, const char*)
    {
        auto& blocking = *static_cast<BlockingContext*>(context);
        blocking.calls.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock lock(blocking.mutex);
        blocking.entered = true;
        blocking.condition.notify_all();
        blocking.condition.wait(lock, [&blocking] { return blocking.release; });
    }

    struct SelfClearContext
    {
        std::atomic_bool called { false };
        std::atomic_bool clear_result { true };
    };

    void SelfClearCallback(void* context, ReaderCGNS_LogLevel, const char*, int, const char*)
    {
        auto& state = *static_cast<SelfClearContext*>(context);
        state.called.store(true, std::memory_order_release);
        state.clear_result.store(ReaderCGNS::Logger::ClearLogCallback(), std::memory_order_release);
    }

    void InspectMissingFile()
    {
        ReaderCGNS::info(std::string(MissingCgnsPath));
    }
} // namespace

int main()
{
    using namespace ReaderCGNS::Logger;

    Check(ClearLogCallback(), "initial callback state can be cleared");
    Check(SetMinimumLogLevel(READER_CGNS_LOG_TRACE), "trace minimum level is accepted");
    Check(GetMinimumLogLevel() == READER_CGNS_LOG_TRACE, "minimum log level can be read back");
    Check(!SetMinimumLogLevel(static_cast<ReaderCGNS_LogLevel>(99)), "invalid minimum log level is rejected");

    CaptureContext capture;
    Check(SetLogCallback(CaptureCallback, &capture), "capture callback can be installed");
    InspectMissingFile();
    Check(ClearLogCallback(), "capture callback can be cleared");
    {
        const std::scoped_lock lock(capture.mutex);
        Check(!capture.records.empty(), "callback receives ReaderCGNS messages");
        if (!capture.records.empty()) {
            Check(capture.records.front().level == READER_CGNS_LOG_INFO, "callback receives a typed log level");
            Check(!capture.records.front().file.empty(), "callback receives a source file");
            Check(capture.records.front().line > 0, "callback receives a source line");
            Check(!capture.records.front().message.empty(), "callback receives a message copied during invocation");
        }
    }

    Check(SetMinimumLogLevel(READER_CGNS_LOG_WARN), "warning minimum level is accepted");
    Check(SetLogCallback(CaptureCallback, &capture), "filtered callback can be installed");
    const std::size_t records_before_filter_test = [&capture] {
        const std::scoped_lock lock(capture.mutex);
        return capture.records.size();
    }();
    InspectMissingFile();
    Check(ClearLogCallback(), "filtered callback can be cleared");
    {
        const std::scoped_lock lock(capture.mutex);
        Check(capture.records.size() == records_before_filter_test, "messages below the minimum level are not dispatched");
    }
    Check(SetMinimumLogLevel(READER_CGNS_LOG_TRACE), "minimum level can be restored");

    std::atomic_size_t throwing_calls { 0 };
    Check(SetLogCallback(ThrowingCallback, &throwing_calls), "throwing callback can be installed");
    InspectMissingFile();
    Check(ClearLogCallback(), "throwing callback can be cleared after exceptions are contained");
    Check(throwing_calls.load(std::memory_order_relaxed) > 0, "throwing callback was invoked without escaping into ReaderCGNS");

    SelfClearContext self_clear;
    Check(SetLogCallback(SelfClearCallback, &self_clear), "self-clearing callback can be installed");
    InspectMissingFile();
    Check(self_clear.called.load(std::memory_order_acquire), "self-clearing callback was invoked");
    Check(!self_clear.clear_result.load(std::memory_order_acquire), "clearing from inside a callback is rejected");
    Check(ClearLogCallback(), "callback can still be cleared externally");

    BlockingContext blocking;
    Check(SetLogCallback(BlockingCallback, &blocking), "blocking callback can be installed");
    std::jthread reader(InspectMissingFile);
    {
        std::unique_lock lock(blocking.mutex);
        blocking.condition.wait(lock, [&blocking] { return blocking.entered; });
    }

    std::promise<void> clear_started;
    std::promise<bool> clear_finished;
    auto clear_started_future = clear_started.get_future();
    auto clear_finished_future = clear_finished.get_future();
    std::jthread clearer([&clear_started, &clear_finished] {
        clear_started.set_value();
        clear_finished.set_value(ReaderCGNS::Logger::ClearLogCallback());
    });
    clear_started_future.wait();
    Check(clear_finished_future.wait_for(100ms) == std::future_status::timeout, "clear waits for an active callback");

    {
        const std::scoped_lock lock(blocking.mutex);
        blocking.release = true;
    }
    blocking.condition.notify_all();
    reader.join();
    clearer.join();
    Check(clear_finished_future.get(), "clear succeeds after the active callback exits");

    const std::size_t calls_after_clear = blocking.calls.load(std::memory_order_relaxed);
    InspectMissingFile();
    Check(blocking.calls.load(std::memory_order_relaxed) == calls_after_clear, "no callback runs after clear returns");

    return failures == 0 ? 0 : 1;
}
