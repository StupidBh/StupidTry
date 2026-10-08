#include "MioReader.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    class TemporaryDirectory {
    public:
        TemporaryDirectory()
        {
            const auto base = std::filesystem::temp_directory_path();
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int attempt = 0; attempt < 32; ++attempt) {
                const auto candidate = base / ("stupid-bhh-mio-" + std::to_string(stamp) + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate)) {
                    this->m_path = candidate;
                    return;
                }
            }
            throw std::runtime_error("Could not create the test directory");
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(this->m_path, error);
        }

        const std::filesystem::path& Path() const { return this->m_path; }

    private:
        std::filesystem::path m_path;
    };

    void Require(bool condition, std::string_view message)
    {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    std::string Filename(const std::filesystem::path& path)
    {
        const auto utf8 = path.u8string();
        return std::string(utf8.begin(), utf8.end());
    }

    void WriteFile(const std::filesystem::path& path, std::string_view text)
    {
        std::ofstream output;
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.open(path, std::ios::binary);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::vector<std::string_view> ReferenceLines(std::string_view text)
    {
        std::vector<std::string_view> lines;
        std::size_t start = 0;
        while (start < text.size()) {
            const auto end = text.find_first_of("\r\n", start);
            if (end == std::string_view::npos) {
                lines.emplace_back(text.substr(start));
                break;
            }
            lines.emplace_back(text.substr(start, end - start));
            start = end + 1;
            if (text[end] == '\r' && start < text.size() && text[start] == '\n') {
                ++start;
            }
        }
        return lines;
    }

    void VerifyReads(const std::filesystem::path& path, std::span<const std::string_view> expected, std::size_t batch_size, bool interleave)
    {
        MioReader reader(Filename(path));
        std::vector<std::string_view> lines;
        std::size_t cursor = 0;
        while (true) {
            lines = { "stale" };
            Require(reader.GetLineBatch(lines, 0) == 0 && lines.empty(), "A zero-sized batch must clear output without advancing");
            const auto count = reader.GetLineBatch(lines, batch_size);
            Require(count == std::min(batch_size, expected.size() - cursor) && count == lines.size(), "Unexpected batch count");
            for (const auto line : lines) {
                Require(line == expected[cursor++], "A batch line differs from the reference parser");
            }
            if (count == 0) {
                break;
            }
            if (interleave) {
                std::string_view line = "sentinel";
                const auto found = reader.GetLine(line);
                Require(found == (cursor < expected.size()), "Unexpected single-line status");
                if (found) {
                    Require(line == expected[cursor++], "A single line differs from the reference parser");
                }
                else {
                    Require(line == "sentinel", "A single read at EOF must preserve its output parameter");
                }
            }
        }
        std::string_view line = "sentinel";
        Require(!reader.GetLine(line) && line == "sentinel", "Unexpected single-line behavior at EOF");
    }

    void CheckText(const std::filesystem::path& path, const std::string& text)
    {
        WriteFile(path, text);
        const auto expected = ReferenceLines(text);
        for (const std::size_t batch_size : { 1, 2, 3, 7, 257, 10000 }) {
            VerifyReads(path, expected, batch_size, false);
            VerifyReads(path, expected, batch_size, true);
        }
    }

    void CheckMixedPatterns(const std::filesystem::path& path)
    {
        std::string text;
        for (int index = 0; index < 2048; ++index) {
            text += "prefix\r";
        }
        constexpr std::array alphabet { 'x', '\r', '\n' };
        std::size_t combinations = 1;
        std::size_t patterns = 0;
        for (std::size_t length = 0; length <= 8; ++length) {
            for (std::size_t number = 0; number < combinations; ++number) {
                text += 'L';
                auto digits = number;
                for (std::size_t index = 0; index < length; ++index) {
                    text += alphabet[digits % alphabet.size()];
                    digits /= alphabet.size();
                }
                text += "R\n";
                ++patterns;
            }
            combinations *= alphabet.size();
        }
        for (const std::string_view ending : { "\r", "\n", "\r\n", "\n\r", "\r\r\n", "\r\n\n" }) {
            text += std::string(8192, 'z');
            text += ending;
        }
        for (int index = 0; index < 512; ++index) {
            text += "suffix\n";
        }
        text += "last line without a delimiter";
        CheckText(path, text);
        std::cout << "Mixed patterns passed: " << patterns << '\n';
    }

    void CheckDefaultAndChangingBatches(const std::filesystem::path& path)
    {
        std::string text;
        for (int index = 0; index < 10005; ++index) {
            text += std::to_string(index);
            text += '\n';
        }
        WriteFile(path, text);
        const auto expected = ReferenceLines(text);
        MioReader reader(Filename(path));
        std::vector<std::string_view> lines;
        Require(reader.GetLineBatch(lines) == 10000, "The default batch size must remain 10000");
        Require(std::equal(lines.begin(), lines.end(), expected.begin()), "Unexpected default batch contents");
        std::size_t cursor = 10000;
        for (const std::size_t batch_size : { 1, 3, 10000 }) {
            const auto count = reader.GetLineBatch(lines, batch_size);
            Require(count == std::min(batch_size, expected.size() - cursor), "Changing batch limits returned the wrong count");
            for (const auto line : lines) {
                Require(line == expected[cursor++], "Changing batch limits lost the reader position");
            }
        }
        Require(cursor == expected.size() && reader.GetLineBatch(lines) == 0 && lines.empty(), "Unexpected default batch behavior at EOF");
    }

    void CheckLongLines(const std::filesystem::path& path)
    {
        constexpr std::size_t width = 64 * 1024;
        constexpr std::size_t count = 256;
        constexpr std::array<std::string_view, 3> endings { "\r", "\n", "\r\n" };
        {
            std::ofstream output;
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output.open(path, std::ios::binary);
            std::string body(width, 'x');
            for (std::size_t index = 0; index < count; ++index) {
                body.assign(width, 'x');
                const auto prefix = std::to_string(index) + ":";
                body.replace(0, prefix.size(), prefix);
                output.write(body.data(), static_cast<std::streamsize>(body.size()));
                if (index + 1 < count) {
                    const auto ending = endings[index % endings.size()];
                    output.write(ending.data(), static_cast<std::streamsize>(ending.size()));
                }
            }
        }
        for (const std::size_t batch_size : { 1, 17, 10000 }) {
            MioReader reader(Filename(path));
            std::vector<std::string_view> lines;
            std::size_t cursor = 0;
            auto check_line = [&](std::string_view line) {
                const auto prefix = std::to_string(cursor++) + ":";
                Require(line.size() == width && line.starts_with(prefix), "A long line was truncated or reordered");
                Require(line.substr(prefix.size()).find_first_not_of('x') == std::string_view::npos, "Long-line bytes changed");
            };
            while (reader.GetLineBatch(lines, batch_size) != 0) {
                for (const auto line : lines) {
                    check_line(line);
                }
                std::string_view single;
                if (reader.GetLine(single)) {
                    check_line(single);
                }
            }
            Require(cursor == count, "Long-line reads lost lines");
        }
    }

    void CheckMoves(const std::filesystem::path& source_path, const std::filesystem::path& target_path)
    {
        WriteFile(source_path, "A\rB\rC\nD");
        WriteFile(target_path, "XY\rZ\nQ");
        auto prime = [&](MioReader& reader, std::string_view expected) {
            std::vector<std::string_view> lines;
            Require(reader.GetLineBatch(lines, 1) == 1 && lines.front() == expected, "Could not prepare a move test");
        };
        auto check_remaining = [&](MioReader& reader) {
            std::vector<std::string_view> lines;
            Require(reader.GetLineBatch(lines) == 3 && lines == std::vector<std::string_view> { "B", "C", "D" }, "A move lost cached reader state");
        };
        {
            MioReader source(Filename(source_path));
            prime(source, "A");
            MioReader destination(std::move(source));
            check_remaining(destination);
        }
        {
            MioReader source(Filename(source_path));
            MioReader destination(Filename(target_path));
            prime(source, "A");
            prime(destination, "XY");
            destination = std::move(source);
            check_remaining(destination);
        }
    }
} // namespace

int main()
{
    try {
        TemporaryDirectory directory;
        const auto input = directory.Path() / "input.txt";
        const std::vector<std::string> cases { "a\nb\nc",
                                               "a\rb\rc",
                                               "a\r\nb\r\nc\r\n",
                                               "\r",
                                               "\n",
                                               "\r\n",
                                               "\n\r",
                                               "\r\r\n",
                                               "\r\n\n",
                                               "\r\n\n\ra\r\nb\rc\n",
                                               "A\rB\r\nC\nD\rE",
                                               "a\n\rb",
                                               std::string("a\0b\r\nc", 6),
                                               "汉字\r\n文本\r末行",
                                               "\xEF\xBB\xBF"
                                               "a\nb",
                                               std::string(4 * 1024 * 1024, 'z') + "\r\nlast" };
        for (const auto& text : cases) {
            CheckText(input, text);
        }
        CheckMixedPatterns(input);
        CheckDefaultAndChangingBatches(input);
        CheckLongLines(input);
        CheckMoves(input, directory.Path() / "move-target.txt");
        std::cout << "MioReader line checks passed\n";
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "MioReader line checks failed: " << error.what() << '\n';
        return 1;
    }
}
