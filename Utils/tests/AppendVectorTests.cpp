#include "Utils/Utils.hpp"

#include <array>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    void Check(bool condition)
    {
        if (!condition) {
            throw std::runtime_error("AppendVector test failed");
        }
    }

    struct ExplicitValue
    {
        explicit ExplicitValue(int input) :
            value(input)
        {
        }

        ExplicitValue(const ExplicitValue&) = default;
        ExplicitValue(ExplicitValue&&) = default;

        int value;
    };

    struct ThrowingCopy
    {
        inline static int copies_until_throw = -1;

        explicit ThrowingCopy(int input) :
            value(input)
        {
        }

        ThrowingCopy(const ThrowingCopy& other) :
            value(other.value)
        {
            if (copies_until_throw == 0) {
                throw std::runtime_error("copy failed");
            }
            if (copies_until_throw > 0) {
                --copies_until_throw;
            }
        }

        ThrowingCopy(ThrowingCopy&&) = default;

        int value;
    };

    void TestVectorSources()
    {
        std::vector<int> target { 1 };
        const std::vector<int> source { 2, 3 };
        utils::AppendVector(target, source);
        Check((target == std::vector<int> { 1, 2, 3 }));

        utils::AppendVector(target, target);
        Check((target == std::vector<int> { 1, 2, 3, 1, 2, 3 }));

        std::vector<int> moving { 4, 5 };
        utils::AppendVector(target, std::move(moving));
        Check((target == std::vector<int> { 1, 2, 3, 1, 2, 3, 4, 5 }));

        bool rejected = false;
        try {
            utils::AppendVector(target, std::move(target));
        }
        catch (const std::invalid_argument&) {
            rejected = true;
        }
        Check(rejected && target.size() == 8);

        std::vector<ExplicitValue> converted;
        const std::vector<int> numbers { 6 };
        utils::AppendVector(converted, numbers);
        std::vector<int> more { 7 };
        utils::AppendVector(converted, std::move(more));
        Check(converted.size() == 2 && converted[0].value == 6 && converted[1].value == 7);

        std::vector<std::unique_ptr<int>> pointers;
        std::vector<std::unique_ptr<int>> incoming;
        incoming.emplace_back(std::make_unique<int>(8));
        utils::AppendVector(pointers, std::move(incoming));
        Check(pointers.size() == 1 && *pointers[0] == 8);
    }

    void TestRangesAndBool()
    {
        std::vector<int> target { 1, 2 };
        utils::AppendVector(target, std::span(target));
        Check((target == std::vector<int> { 1, 2, 1, 2 }));
        utils::AppendVector(target, std::views::iota(3, 5));
        Check((target == std::vector<int> { 1, 2, 1, 2, 3, 4 }));

        std::vector<bool> bits { true };
        const std::vector<bool> copied { false };
        utils::AppendVector(bits, copied);
        utils::AppendVector(bits, bits);
        std::vector<bool> moved { true };
        utils::AppendVector(bits, std::move(moved));
        const std::array<bool, 1> range { false };
        utils::AppendVector(bits, range);
        utils::AppendVector(bits, 2, true);
        Check((bits == std::vector<bool> { true, false, true, false, true, false, true, true }));
    }

    void TestCountAndFailure()
    {
        std::vector<int> target { 9 };
        utils::AppendVector(target, 2, target.front());
        utils::AppendVector(target, 0, 1);
        Check((target == std::vector<int> { 9, 9, 9 }));

        std::vector<ThrowingCopy> values;
        values.reserve(8);
        values.emplace_back(1);
        ThrowingCopy::copies_until_throw = 2;
        bool threw = false;
        try {
            utils::AppendVector(values, 3, values.front());
        }
        catch (const std::runtime_error&) {
            threw = true;
        }
        Check(threw && values.size() == 1 && values.front().value == 1);
        ThrowingCopy::copies_until_throw = -1;
    }
} // namespace

int main()
{
    TestVectorSources();
    TestRangesAndBool();
    TestCountAndFailure();
}
