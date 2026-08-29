#include "ReaderAPI/ReaderCGNS.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <latch>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <windows.h>

namespace {
    using namespace std::chrono_literals;
    using ReaderAPI::Logger::LogCallback;
    using ReaderAPI::Logger::LogLevel;

    constexpr std::string_view MissingCgnsPath = "ReaderCGNS-logger-callback-test-missing.cgns";
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

    struct ReaderDeleter
    {
        ReaderAPI::DestroyReaderCGNSFunc destroy = nullptr;

        void operator()(ReaderAPI::ReaderCGNS* reader) const noexcept
        {
            if (reader != nullptr && this->destroy != nullptr) {
                this->destroy(reader);
            }
        }
    };

    using ReaderPtr = std::unique_ptr<ReaderAPI::ReaderCGNS, ReaderDeleter>;

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

    ReaderPtr CreateReader(const ReaderAPI::CreateReaderCGNSFunc create, const ReaderAPI::DestroyReaderCGNSFunc destroy)
    {
        return ReaderPtr(create(), ReaderDeleter { destroy });
    }

    void InspectMissingFile(ReaderAPI::ReaderCGNS& reader)
    {
        reader.Open(std::string(MissingCgnsPath));
    }

    struct LogRecord
    {
        LogLevel level;
        std::string file;
        int line;
        std::string message;
    };

    struct CaptureContext
    {
        std::mutex mutex;
        std::vector<LogRecord> records;
    };

    void CaptureCallback(void* context, const LogLevel level, const char* file, const int line, const char* message)
    {
        auto& capture = *static_cast<CaptureContext*>(context);
        const std::scoped_lock lock(capture.mutex);
        capture.records.push_back(LogRecord { level, file, line, message });
    }

    std::size_t RecordCount(CaptureContext& capture)
    {
        const std::scoped_lock lock(capture.mutex);
        return capture.records.size();
    }

    void CheckFirstRecord(CaptureContext& capture)
    {
        const std::scoped_lock lock(capture.mutex);
        Check(!capture.records.empty(), "callback receives ReaderCGNS messages");
        if (capture.records.empty()) {
            return;
        }

        const auto& record = capture.records.front();
        Check(record.level == ReaderAPI::Logger::READER_CGNS_LOG_INFO, "callback receives a typed log level");
        Check(!record.file.empty(), "callback receives a source file");
        Check(record.line > 0, "callback receives a source line");
        Check(!record.message.empty(), "callback receives a message copied during invocation");
    }

    void ThrowingCallback(void* context, LogLevel, const char*, int, const char*)
    {
        auto& calls = *static_cast<std::atomic_size_t*>(context);
        if (calls.fetch_add(1, std::memory_order_relaxed) == 0) {
            throw std::runtime_error("callback failure");
        }
    }

    struct BlockingContext
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool entered = false;
        bool release = false;
        std::atomic_size_t calls { 0 };
    };

    void BlockingCallback(void* context, LogLevel, const char*, int, const char*)
    {
        auto& blocking = *static_cast<BlockingContext*>(context);
        blocking.calls.fetch_add(1, std::memory_order_relaxed);

        std::unique_lock lock(blocking.mutex);
        blocking.entered = true;
        blocking.condition.notify_all();
        blocking.condition.wait(lock, [&blocking] { return blocking.release; });
    }

    struct ReentrantContext
    {
        ReaderAPI::ReaderCGNS* current = nullptr;
        ReaderAPI::ReaderCGNS* bound_other = nullptr;
        ReaderAPI::ReaderCGNS* unbound_other = nullptr;
        std::atomic_size_t calls { 0 };
        std::atomic_bool set_current_result { true };
        std::atomic_bool clear_current_result { true };
        std::atomic_bool clear_bound_other_result { true };
        std::atomic_bool set_unbound_other_result { true };
        std::atomic_bool clear_unbound_other_result { true };
    };

    void ReentrantCallback(void* context, LogLevel, const char*, int, const char*)
    {
        auto& reentrant = *static_cast<ReentrantContext*>(context);
        if (reentrant.calls.fetch_add(1, std::memory_order_relaxed) != 0) {
            return;
        }

        reentrant.set_current_result.store(reentrant.current->SetLogCallback(CaptureCallback, nullptr), std::memory_order_release);
        reentrant.clear_current_result.store(reentrant.current->ClearLogCallback(), std::memory_order_release);
        reentrant.clear_bound_other_result.store(reentrant.bound_other->ClearLogCallback(), std::memory_order_release);
        reentrant.set_unbound_other_result.store(reentrant.unbound_other->SetLogCallback(CaptureCallback, nullptr), std::memory_order_release);
        reentrant.clear_unbound_other_result.store(reentrant.unbound_other->ClearLogCallback(), std::memory_order_release);
    }
} // namespace

int main()
{
    const ModuleGuard module;
    Check(module.module != nullptr, "ReaderCGNS.dll can be loaded");
    if (module.module == nullptr) {
        return 1;
    }

    const auto create = LoadExport<ReaderAPI::CreateReaderCGNSFunc>(module.module, "CreateReaderCGNS");
    const auto destroy = LoadExport<ReaderAPI::DestroyReaderCGNSFunc>(module.module, "DestroyReaderCGNS");
    Check(create != nullptr, "the reader factory export can be resolved");
    Check(destroy != nullptr, "the reader destroy export can be resolved");
    Check(GetProcAddress(module.module, "SetLogCallback") == nullptr, "the process-level set-log export was removed");
    Check(GetProcAddress(module.module, "ClearLogCallback") == nullptr, "the process-level clear-log export was removed");
    if (create == nullptr || destroy == nullptr) {
        return 1;
    }

    ReaderPtr reader_a = CreateReader(create, destroy);
    ReaderPtr reader_b = CreateReader(create, destroy);
    ReaderPtr reader_c = CreateReader(create, destroy);
    Check(reader_a != nullptr && reader_b != nullptr && reader_c != nullptr, "the factory creates independent reader instances");
    if (reader_a == nullptr || reader_b == nullptr || reader_c == nullptr) {
        return 1;
    }

    Check(reader_a->ClearLogCallback(), "clearing an unbound reader is idempotent");
    Check(!reader_a->SetLogCallback(nullptr, nullptr), "a null callback is rejected while unbound");

    CaptureContext capture_a;
    CaptureContext replacement_a;
    CaptureContext capture_b;
    Check(reader_a->SetLogCallback(CaptureCallback, &capture_a), "reader A installs its callback");
    Check(!reader_a->SetLogCallback(nullptr, &replacement_a), "a null callback is rejected while bound");
    Check(!reader_a->SetLogCallback(CaptureCallback, &replacement_a), "reader A rejects callback replacement");

    InspectMissingFile(*reader_a);
    CheckFirstRecord(capture_a);
    Check(RecordCount(replacement_a) == 0, "failed registration preserves reader A's original context");

    Check(reader_b->SetLogCallback(CaptureCallback, &capture_b), "reader B installs its callback");
    const std::size_t reader_a_before_b = RecordCount(capture_a);
    InspectMissingFile(*reader_b);
    Check(RecordCount(capture_b) > 0, "reader B receives its own messages");
    Check(RecordCount(capture_a) == reader_a_before_b, "reader B does not dispatch to reader A's callback");

    Check(reader_a->ClearLogCallback(), "reader A clears its callback");
    const std::size_t reader_a_after_clear = RecordCount(capture_a);
    const std::size_t reader_b_before_a_clear_check = RecordCount(capture_b);
    InspectMissingFile(*reader_a);
    InspectMissingFile(*reader_b);
    Check(RecordCount(capture_a) == reader_a_after_clear, "reader A stops dispatching after clear returns");
    Check(RecordCount(capture_b) > reader_b_before_a_clear_check, "clearing reader A does not affect reader B");
    Check(reader_a->ClearLogCallback(), "repeated clear remains successful");
    Check(reader_a->SetLogCallback(CaptureCallback, &capture_a), "reader A can register again after clear");
    Check(reader_a->ClearLogCallback(), "reader A clears its rebound callback");
    Check(reader_b->ClearLogCallback(), "reader B clears its callback");

    CaptureContext reentrant_bound_capture;
    ReentrantContext reentrant { .current = reader_a.get(), .bound_other = reader_b.get(), .unbound_other = reader_c.get() };
    Check(reader_a->SetLogCallback(ReentrantCallback, &reentrant), "the reentrant callback can be installed");
    Check(reader_b->SetLogCallback(CaptureCallback, &reentrant_bound_capture), "the reentrant test binds a second reader");
    InspectMissingFile(*reader_a);
    Check(reentrant.calls.load(std::memory_order_acquire) > 0, "the reentrant callback was invoked");
    Check(!reentrant.set_current_result.load(std::memory_order_acquire), "Set on the current reader is rejected inside a callback");
    Check(!reentrant.clear_current_result.load(std::memory_order_acquire), "Clear on the current reader is rejected inside a callback");
    Check(!reentrant.clear_bound_other_result.load(std::memory_order_acquire), "Clear on another bound reader is rejected inside a callback");
    Check(!reentrant.set_unbound_other_result.load(std::memory_order_acquire), "Set on another unbound reader is rejected inside a callback");
    Check(!reentrant.clear_unbound_other_result.load(std::memory_order_acquire), "Clear on another unbound reader is rejected inside a callback");
    InspectMissingFile(*reader_b);
    Check(RecordCount(reentrant_bound_capture) > 0, "rejected reentrant operations preserve the other reader's binding");
    Check(reader_c->ClearLogCallback(), "the unbound reader remains clear after rejected reentrant operations");
    Check(reader_a->ClearLogCallback(), "the reentrant callback can be cleared externally");
    Check(reader_b->ClearLogCallback(), "the second reentrant-test reader can be cleared externally");

    CaptureContext concurrent_first;
    CaptureContext concurrent_second;
    std::latch registration_start(3);
    std::atomic_bool first_registration { false };
    std::atomic_bool second_registration { false };
    std::jthread first_registrar([&] {
        registration_start.arrive_and_wait();
        first_registration.store(reader_c->SetLogCallback(CaptureCallback, &concurrent_first), std::memory_order_release);
    });
    std::jthread second_registrar([&] {
        registration_start.arrive_and_wait();
        second_registration.store(reader_c->SetLogCallback(CaptureCallback, &concurrent_second), std::memory_order_release);
    });
    registration_start.arrive_and_wait();
    first_registrar.join();
    second_registrar.join();
    const bool first_succeeded = first_registration.load(std::memory_order_acquire);
    const bool second_succeeded = second_registration.load(std::memory_order_acquire);
    Check(first_succeeded != second_succeeded, "only one concurrent registration succeeds for a reader");
    InspectMissingFile(*reader_c);
    Check((RecordCount(concurrent_first) > 0) == first_succeeded, "the first concurrent context matches its registration result");
    Check((RecordCount(concurrent_second) > 0) == second_succeeded, "the second concurrent context matches its registration result");
    Check(reader_c->ClearLogCallback(), "the concurrently selected callback can be cleared");

    std::atomic_size_t throwing_calls { 0 };
    Check(reader_a->SetLogCallback(ThrowingCallback, &throwing_calls), "a throwing callback can be installed");
    bool callback_exception_escaped = false;
    try {
        InspectMissingFile(*reader_a);
    }
    catch (...) {
        callback_exception_escaped = true;
    }
    Check(!callback_exception_escaped, "callback exceptions do not cross the DLL boundary");
    Check(throwing_calls.load(std::memory_order_relaxed) > 0, "the throwing callback was invoked");
    Check(reader_a->ClearLogCallback(), "the throwing callback can be cleared");

    BlockingContext blocking;
    Check(reader_a->SetLogCallback(BlockingCallback, &blocking), "a blocking callback can be installed");
    std::jthread inspecting_thread([&] { InspectMissingFile(*reader_a); });
    bool blocking_callback_entered = false;
    {
        std::unique_lock lock(blocking.mutex);
        blocking_callback_entered = blocking.condition.wait_for(lock, 5s, [&blocking] { return blocking.entered; });
    }
    Check(blocking_callback_entered, "the blocking callback is entered");

    if (blocking_callback_entered) {
        std::promise<void> clear_started;
        std::promise<bool> clear_finished;
        auto clear_started_future = clear_started.get_future();
        auto clear_finished_future = clear_finished.get_future();
        std::jthread clearing_thread([&] {
            clear_started.set_value();
            clear_finished.set_value(reader_a->ClearLogCallback());
        });
        clear_started_future.wait();
        Check(clear_finished_future.wait_for(100ms) == std::future_status::timeout, "clear waits for an active callback");

        {
            const std::scoped_lock lock(blocking.mutex);
            blocking.release = true;
        }
        blocking.condition.notify_all();
        inspecting_thread.join();
        clearing_thread.join();
        Check(clear_finished_future.get(), "clear succeeds after the active callback exits");
    }
    else {
        {
            const std::scoped_lock lock(blocking.mutex);
            blocking.release = true;
        }
        blocking.condition.notify_all();
        inspecting_thread.join();
        Check(reader_a->ClearLogCallback(), "the blocking callback is cleared after a failed wait setup");
    }

    const std::size_t blocking_calls_after_clear = blocking.calls.load(std::memory_order_relaxed);
    InspectMissingFile(*reader_a);
    Check(blocking.calls.load(std::memory_order_relaxed) == blocking_calls_after_clear, "no callback runs after clear returns");

    CaptureContext destroy_capture;
    ReaderPtr destroy_reader = CreateReader(create, destroy);
    Check(destroy_reader != nullptr, "a reader can be created for destructor cleanup");
    if (destroy_reader != nullptr) {
        Check(destroy_reader->SetLogCallback(CaptureCallback, &destroy_capture), "the destructor-cleanup reader installs a callback");
        InspectMissingFile(*destroy_reader);
        const std::size_t records_before_destroy = RecordCount(destroy_capture);
        destroy_reader.reset();
        Check(RecordCount(destroy_capture) == records_before_destroy, "destroying a reader does not invoke its callback after cleanup");
    }

    return failures == 0 ? 0 : 1;
}
