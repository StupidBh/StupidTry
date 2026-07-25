#include "Functions.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
    int g_failures = 0;

    void Check(bool condition, std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++g_failures;
        }
    }
}

int main()
{
    const std::string utf8_chinese("\xE4\xB8\xAD\xE6\x96\x87");
    const std::string gbk_chinese("\xD6\xD0\xCE\xC4");
    const std::string invalid_encoding("\xFF");

    Check(IsValidUTF8("plain ASCII"), "ASCII is valid UTF-8");
    Check(IsValidUTF8(utf8_chinese), "Chinese UTF-8 is valid");
    Check(IsLikelyGBK(utf8_chinese), "regression input remains ambiguous with GBK");
    Check(NormalizeToUTF8(utf8_chinese) == utf8_chinese, "valid UTF-8 is preserved");

    Check(!IsValidUTF8(gbk_chinese), "GBK sample is not valid UTF-8");
    Check(IsLikelyGBK(gbk_chinese), "GBK sample is detected");
    Check(GBKToUTF8(gbk_chinese) == utf8_chinese, "GBK converts to UTF-8");
    Check(NormalizeToUTF8(gbk_chinese) == utf8_chinese, "GBK normalization converts to UTF-8");
    Check(NormalizeToUTF8(invalid_encoding) == invalid_encoding, "unknown encoding is preserved");

    Check(FindCaseInsensitive("AlphaBeta", "BETA") == 5, "case-insensitive search");
    Check(FindCaseInsensitive("Alpha", "z") == std::string_view::npos, "case-insensitive search miss");
    Check(IEquals("Alpha", "aLPHa"), "case-insensitive equality");
    Check(!IEquals("Alpha", "Alphas"), "case-insensitive length mismatch");
    Check(TrimSpaces(" \t value \r\n") == "value", "trim ASCII whitespace");
    Check(StripEdgeChar("\"value\"", '"') == "value", "strip matching edge characters");
    Check(StripEdgeChar("\"", '"').empty(), "strip a single matching character");

    Check(!GetExecutablePath().empty(), "executable path is available");
    Check(!GetExecutableDirectory().empty(), "executable directory is available");
    Check(!GetEnv("SystemRoot").empty(), "environment variable is available");

    std::vector<std::string> command_output;
    CallCmd(R"(cmd.exe /d /s /c "echo Alpha&&echo Beta")", [&](const std::string& line) {
        command_output.emplace_back(line);
        return false;
    });
    Check(command_output == std::vector<std::string> { "Alpha", "Beta" }, "command output is split into normalized lines");

    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
