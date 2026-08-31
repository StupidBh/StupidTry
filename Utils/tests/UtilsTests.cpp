#include "Utils/BlockingQueue.hpp"
#include "Utils/ScopedTimer.hpp"
#include "Utils/SingletonHolder.hpp"
#include "Utils/SmartPrefixSum.hpp"
#include "Utils/SyncController.hpp"
#include "Utils/ThreadPool.hpp"
#include "Utils/Utils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    int failures = 0;

    void Check(const bool condition, const std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAILED: " << message << std::endl;
            ++failures;
        }
    }

    class TestSingleton final : public utils::SingletonHolder<TestSingleton> {
        friend class utils::SingletonHolder<TestSingleton>;

        TestSingleton() = default;

    public:
        ~TestSingleton() = default;
        int value = 0;
    };

    struct MoveConstructOnly
    {
        explicit MoveConstructOnly(const int initial_value) :
            value(initial_value)
        {
        }

        MoveConstructOnly(const MoveConstructOnly&) = delete;
        MoveConstructOnly& operator=(const MoveConstructOnly&) = delete;
        MoveConstructOnly(MoveConstructOnly&&) noexcept = default;
        MoveConstructOnly& operator=(MoveConstructOnly&&) = delete;

        int value;
    };

    struct ShrinkableProbe
    {
        void shrink_to_fit() { ++calls; }

        int calls = 0;
    };

    template<class... Types>
    concept CanVectorShrink = requires(Types&... values) { utils::VectorShrink(values...); };

    static_assert(utils::Singleton<TestSingleton>);
    static_assert(!std::is_polymorphic_v<TestSingleton>);
    static_assert(std::same_as<utils::PrefixSumType_t<float>, double>);
    static_assert(std::same_as<utils::PrefixSumType_t<long double>, long double>);
    static_assert(!std::is_constructible_v<utils::SmartPrefixSum<int>, std::vector<int>&&>);
    static_assert(CanVectorShrink<std::vector<int>>);
    static_assert(!CanVectorShrink<const std::vector<int>>);
    static_assert(!CanVectorShrink<int>);
    static_assert(!CanVectorShrink<>);

    void TestContainerUtilities()
    {
        const double infinity = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        Check(utils::almost_equal(infinity, infinity), "equal infinities compare equal");
        Check(!utils::almost_equal(infinity, -infinity), "opposite infinities do not compare equal");
        Check(!utils::almost_equal(nan, nan), "NaN does not compare equal");

        std::vector<int> self_append { 1, 2 };
        utils::AppendVector(self_append, self_append);
        Check(self_append == std::vector<int>({ 1, 2, 1, 2 }), "vector can append itself safely");

        std::vector<int> aliased_range { 3, 4 };
        const std::span<const int> source_view(aliased_range);
        utils::AppendVector(aliased_range, source_view);
        Check(aliased_range == std::vector<int>({ 3, 4, 3, 4 }), "aliased range is materialized before append");

        std::vector<double> converted_target { 0.5 };
        const std::vector<int> converted_source { 1, 2 };
        utils::AppendVector(converted_target, converted_source);
        utils::AppendVector(converted_target, std::vector<int> { 3, 4 });
        Check(converted_target == std::vector<double>({ 0.5, 1.0, 2.0, 3.0, 4.0 }), "vectors with convertible element types can be appended");

        std::vector<MoveConstructOnly> move_target;
        move_target.emplace_back(1);
        std::vector<MoveConstructOnly> move_source;
        move_source.emplace_back(2);
        move_source.emplace_back(3);
        utils::AppendVector(move_target, std::move(move_source));
        Check(move_target.size() == 3 && move_target[0].value == 1 && move_target[1].value == 2 && move_target[2].value == 3,
              "append only requires element move construction");

        bool rejected_move_to_self = false;
        try {
            utils::AppendVector(aliased_range, std::move(aliased_range));
        }
        catch (const std::invalid_argument&) {
            rejected_move_to_self = true;
        }
        Check(rejected_move_to_self, "moving a vector into itself is rejected");

        std::vector<std::string> repeated;
        std::string source = "value";
        utils::AppendVector(repeated, 3, std::move(source));
        Check(repeated == std::vector<std::string>({ "value", "value", "value" }), "repeated append stabilizes an rvalue");

        Check(utils::CreateVector(4, 2, 3) == std::vector<int>({ 2, 5, 8, 11 }), "CreateVector builds a sequence");
        const auto converted = utils::ShrinkVector<double>(std::vector<int> { 1, 2, 3 });
        Check(converted == std::vector<double>({ 1.0, 2.0, 3.0 }), "ShrinkVector converts elements");

        auto clearable = std::vector<int> { 1, 2, 3 };
        utils::DeepClear(clearable);
        Check(clearable.empty(), "DeepClear resets a container");

        ShrinkableProbe first_probe;
        ShrinkableProbe second_probe;
        utils::VectorShrink(first_probe, second_probe);
        Check(first_probe.calls == 1 && second_probe.calls == 1, "VectorShrink invokes every supported argument");
    }

    void TestScopedTimer()
    {
        using MillisecondTimer = utils::ScopedTimer<std::chrono::milliseconds>;

        std::string output;
        MillisecondTimer timer("milliseconds", [&](const std::string_view message) { output = message; });
        Check(timer.elapsed().count() >= 0, "integral duration can be queried");
        timer.stop();
        timer.stop();
        Check(output.starts_with("[milliseconds] Execution time: "), "timer callback receives a named message");
        Check(output.ends_with('s'), "timer output uses seconds");

        std::string move_only_output;
        MillisecondTimer move_only_timer("move-only", [state = std::make_unique<int>(0), &move_only_output](const std::string_view message) mutable {
            ++*state;
            move_only_output = message;
        });
        move_only_timer.stop();
        Check(move_only_output.starts_with("[move-only] Execution time: "), "timer accepts a move-only callback");
    }

    void TestSmartPrefixSum()
    {
        std::vector<int> values(2000, 1);
        utils::SmartPrefixSum<int> prefix_sum(values);
        Check(prefix_sum.query(1500) == 1501, "prefix sum calculates an initial range");
        Check(prefix_sum.query(1600) == 1601, "prefix sum advances its cache");
        Check(prefix_sum.query(1200) == 1201, "prefix sum rewinds its cache");

        values.front() = 2;
        prefix_sum.invalidate();
        Check(prefix_sum.query(1200) == 1202, "invalidation observes source changes");

        values.push_back(1);
        Check(prefix_sum.query(2000) == 2002, "vector-backed prefix sum observes appended values");
        Check(!prefix_sum.try_query(values.size()).has_value(), "try_query distinguishes an out-of-range index");
        Check(prefix_sum.query(values.size()) == 0, "query preserves the zero result for an out-of-range index");

        values.resize(1000);
        Check(prefix_sum.query(999) == 1001, "shrinking the source invalidates an out-of-range cache");

        const std::array fixed_values { 1, 2, 3, 4 };
        utils::PrefixSumPolicy sequenced_policy;
        sequenced_policy.execution = utils::PrefixSumExecution::sequenced;
        utils::SmartPrefixSum<int> fixed_prefix_sum(fixed_values, sequenced_policy);
        Check(fixed_prefix_sum.try_query(3) == std::optional<std::int64_t>(10), "prefix sum accepts a borrowed contiguous range");
    }

    void TestSyncController()
    {
        utils::SyncController controller;
        Check(controller.wait_for(utils::SyncController::Side::producer), "producer state is initially ready");
        controller.mark_ready();
        Check(controller.wait_for(utils::SyncController::Side::consumer), "consumer state is ready after data is marked ready");
        controller.mark_consumed();
        Check(controller.wait_for(utils::SyncController::Side::producer), "producer state resumes after data is consumed");

        const std::stop_token stop_token = controller.get_stop_token();
        controller.stop();
        Check(controller.is_stopped(), "stop state is observable");
        Check(stop_token.stop_requested(), "external stop token observes controller shutdown");
        Check(!controller.wait_for([] { return false; }), "predicate wait reports interruption");
        controller.init();
        Check(controller.wait_for([] { return true; }), "predicate wait reports success");

        utils::SyncController blocked_controller;
        std::atomic_bool wait_result = true;
        std::jthread waiter([&] { wait_result.store(blocked_controller.wait_for(utils::SyncController::Side::consumer)); });
        blocked_controller.stop();
        waiter.join();
        Check(!wait_result.load(), "stop interrupts a blocked side wait");
    }

    void TestBlockingQueue()
    {
        utils::BlockingQueue<std::unique_ptr<int>> queue(1);
        Check(queue.Push(std::make_unique<int>(42)), "queue accepts move-only values");
        queue.Close();
        auto value = queue.Pop();
        Check(value && **value == 42, "closed queue drains existing values");
        Check(!queue.Pop().has_value(), "drained queue returns nullopt");
        auto rejected_value = std::make_unique<int>(7);
        Check(!queue.Push(std::move(rejected_value)), "closed queue rejects new values");
        Check(rejected_value != nullptr, "a rejected push does not consume its rvalue");

        utils::BlockingQueue<std::string> emplace_queue(1);
        Check(emplace_queue.Emplace(3, 'x'), "queue constructs values in place");
        Check(emplace_queue.Pop() == std::optional<std::string>("xxx"), "in-place value can be popped");

        utils::BlockingQueue<int> cancelled_pop_queue;
        std::stop_source pop_stop_source;
        std::optional<int> cancelled_pop_result = 1;
        std::jthread pop_waiter([&] { cancelled_pop_result = cancelled_pop_queue.Pop(pop_stop_source.get_token()); });
        pop_stop_source.request_stop();
        pop_waiter.join();
        Check(!cancelled_pop_result.has_value(), "stop token cancels a blocked pop");

        utils::BlockingQueue<int> pre_cancelled_pop_queue;
        Check(pre_cancelled_pop_queue.Push(9), "queue accepts a value before pop cancellation");
        std::stop_source pre_cancelled_pop_source;
        pre_cancelled_pop_source.request_stop();
        Check(!pre_cancelled_pop_queue.Pop(pre_cancelled_pop_source.get_token()).has_value(), "pre-cancelled pop does not consume ready data");
        Check(pre_cancelled_pop_queue.Pop() == std::optional<int>(9), "ready data remains after a pre-cancelled pop");

        utils::BlockingQueue<std::unique_ptr<int>> pre_cancelled_push_queue;
        std::stop_source pre_cancelled_push_source;
        pre_cancelled_push_source.request_stop();
        auto pre_cancelled_push_value = std::make_unique<int>(3);
        Check(!pre_cancelled_push_queue.Push(pre_cancelled_push_source.get_token(), std::move(pre_cancelled_push_value)),
              "pre-cancelled push does not use available capacity");
        Check(pre_cancelled_push_value != nullptr && pre_cancelled_push_queue.Size() == 0, "pre-cancelled push preserves the value and queue");

        utils::BlockingQueue<std::unique_ptr<int>> cancelled_push_queue(1);
        Check(cancelled_push_queue.Push(std::make_unique<int>(1)), "bounded queue accepts its first value");
        std::stop_source push_stop_source;
        auto cancelled_push_value = std::make_unique<int>(2);
        bool cancelled_push_result = true;
        std::jthread push_waiter(
            [&] { cancelled_push_result = cancelled_push_queue.Push(push_stop_source.get_token(), std::move(cancelled_push_value)); });
        push_stop_source.request_stop();
        push_waiter.join();
        Check(!cancelled_push_result, "stop token cancels a blocked push");
        Check(cancelled_push_value != nullptr, "a cancelled push does not consume its rvalue");
    }

    void TestThreadPool()
    {
        utils::ThreadPool pool(2);
        int referenced_value = 3;
        auto reference_result = pool.enqueue(
            [](int& value, const int increment) {
                value += increment;
                return value;
            },
            std::ref(referenced_value),
            4);
        Check(reference_result.get() == 7 && referenced_value == 7, "std::ref preserves reference semantics");

        auto move_only_result = pool.enqueue([](std::unique_ptr<int> value) { return *value; }, std::make_unique<int>(9));
        Check(move_only_result.get() == 9, "thread pool accepts move-only arguments");

        auto move_only_callable = pool.enqueue([value = std::make_unique<int>(11)] { return *value; });
        Check(move_only_callable.get() == 11, "thread pool accepts a move-only callable");

        auto throwing_task = pool.enqueue([]() -> int { throw std::runtime_error("task failure"); });
        bool task_exception_observed = false;
        try {
            static_cast<void>(throwing_task.get());
        }
        catch (const std::runtime_error&) {
            task_exception_observed = true;
        }
        Check(task_exception_observed, "task exceptions are delivered through the future");

        auto guarded_wait = pool.enqueue([&pool] {
            try {
                pool.wait_for_completion();
            }
            catch (const std::logic_error&) {
                return true;
            }
            return false;
        });
        Check(guarded_wait.get(), "task cannot wait for its own pool");
        pool.wait_for_completion();
        pool.shutdown();

        bool rejected_after_shutdown = false;
        try {
            static_cast<void>(pool.enqueue([] { return 0; }));
        }
        catch (const std::runtime_error&) {
            rejected_after_shutdown = true;
        }
        Check(rejected_after_shutdown, "stopped pool rejects tasks");

        const auto caller = std::this_thread::get_id();
        utils::ThreadPool inline_pool(0);
        Check(inline_pool.enqueue([] { return std::this_thread::get_id(); }).get() == caller, "zero-thread pool runs inline");

        utils::ThreadPool concurrent_shutdown_pool(1);
        std::latch shutdown_task_started(1);
        std::latch release_shutdown_task(1);
        auto shutdown_task = concurrent_shutdown_pool.enqueue([&] {
            shutdown_task_started.count_down();
            release_shutdown_task.wait();
        });
        shutdown_task_started.wait();

        std::jthread first_shutdown([&] { concurrent_shutdown_pool.shutdown(); });
        while (true) {
            try {
                static_cast<void>(concurrent_shutdown_pool.enqueue([] { }));
                std::this_thread::yield();
            }
            catch (const std::runtime_error&) {
                break;
            }
        }

        std::promise<void> second_shutdown_finished;
        auto second_shutdown_result = second_shutdown_finished.get_future();
        std::jthread second_shutdown([&] {
            concurrent_shutdown_pool.shutdown();
            second_shutdown_finished.set_value();
        });
        Check(second_shutdown_result.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout,
              "concurrent shutdown waits for the active shutdown");
        release_shutdown_task.count_down();
        first_shutdown.join();
        second_shutdown.join();
        shutdown_task.get();
        Check(second_shutdown_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready, "all shutdown callers complete together");

        utils::ThreadPool cancelling_pool(1);
        std::latch active_task_started(1);
        std::latch release_active_task(1);
        auto active_task = cancelling_pool.enqueue([&] {
            active_task_started.count_down();
            release_active_task.wait();
            return 1;
        });
        active_task_started.wait();
        auto discarded_task = cancelling_pool.enqueue([] { return 2; });
        std::jthread cancelling_thread([&] { cancelling_pool.shutdown_now(); });
        Check(discarded_task.wait_for(std::chrono::seconds(1)) == std::future_status::ready, "immediate shutdown discards queued tasks promptly");
        bool broken_promise_observed = false;
        try {
            static_cast<void>(discarded_task.get());
        }
        catch (const std::future_error& error) {
            broken_promise_observed = error.code() == std::make_error_code(std::future_errc::broken_promise);
        }
        Check(broken_promise_observed, "discarded task reports a broken promise");
        release_active_task.count_down();
        cancelling_thread.join();
        Check(active_task.get() == 1, "immediate shutdown lets an active task finish");
    }

    void TestSingletonHolder()
    {
        auto& first = TestSingleton::get_instance();
        auto& second = TestSingleton::get_instance();
        first.value = 17;
        Check(std::addressof(first) == std::addressof(second), "SingletonHolder returns one instance");
        Check(second.value == 17, "SingletonHolder preserves state");

        std::array<TestSingleton*, 4> instances { };
        std::array<std::jthread, 4> threads;
        for (std::size_t index = 0; index < threads.size(); ++index) {
            threads[index] = std::jthread([&, index] { instances[index] = std::addressof(TestSingleton::get_instance()); });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        Check(std::ranges::all_of(instances, [&first](const TestSingleton* instance) { return instance == std::addressof(first); }),
              "SingletonHolder initialization is stable across threads");
    }
} // namespace

int main()
{
    TestContainerUtilities();
    TestScopedTimer();
    TestSmartPrefixSum();
    TestSyncController();
    TestBlockingQueue();
    TestThreadPool();
    TestSingletonHolder();

    if (failures != 0) {
        std::cerr << failures << " Utils test(s) failed" << std::endl;
        return 1;
    }
    return 0;
}
