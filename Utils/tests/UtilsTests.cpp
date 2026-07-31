#include "Utils/BlockingQueue.hpp"
#include "Utils/ScopedTimer.hpp"
#include "Utils/SingletonHolder.hpp"
#include "Utils/SmartPrefixSum.hpp"
#include "Utils/SyncController.hpp"
#include "Utils/ThreadPool.hpp"
#include "Utils/Utils.hpp"

#include <chrono>
#include <concepts>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
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

    static_assert(utils::Singleton<TestSingleton>);
    static_assert(!std::is_polymorphic_v<TestSingleton>);
    static_assert(std::same_as<utils::PrefixSumType_t<float>, double>);
    static_assert(std::same_as<utils::PrefixSumType_t<long double>, long double>);
    static_assert(!std::is_constructible_v<utils::SmartPrefixSum<int>, std::vector<int>&&>);

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
    }

    void TestSyncAndQueue()
    {
        utils::SyncController controller;
        Check(controller.wait_for(false), "producer state is initially ready");
        controller.toggle_ready_and_notify_all();
        Check(controller.wait_for(true), "consumer state is ready after a toggle");
        controller.stop();
        Check(controller.is_stopped(), "stop state is observable");
        Check(!controller.wait_for([] { return false; }), "predicate wait reports interruption");
        controller.init();
        Check(controller.wait_for([] { return true; }), "predicate wait reports success");

        utils::BlockingQueue<std::unique_ptr<int>> queue(1);
        Check(queue.Push(std::make_unique<int>(42)), "queue accepts move-only values");
        queue.Close();
        auto value = queue.Pop();
        Check(value && **value == 42, "closed queue drains existing values");
        Check(!queue.Pop().has_value(), "drained queue returns nullopt");
        Check(!queue.Push(std::make_unique<int>(7)), "closed queue rejects new values");
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
    }

    void TestSingletonHolder()
    {
        auto& first = TestSingleton::get_instance();
        auto& second = TestSingleton::get_instance();
        first.value = 17;
        Check(std::addressof(first) == std::addressof(second), "SingletonHolder returns one instance");
        Check(second.value == 17, "SingletonHolder preserves state");
    }
} // namespace

int main()
{
    TestContainerUtilities();
    TestScopedTimer();
    TestSmartPrefixSum();
    TestSyncAndQueue();
    TestThreadPool();
    TestSingletonHolder();

    if (failures != 0) {
        std::cerr << failures << " Utils test(s) failed" << std::endl;
        return 1;
    }
    return 0;
}
