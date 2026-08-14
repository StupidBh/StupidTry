#include "Functions.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
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

int main(int argc, char* argv[])
{
    if (argc >= 2 && std::string_view(argv[1]) == "--call-cmd-probe") {
        for (int i = 2; i < argc; ++i) {
            std::cout << "arg=" << argv[i] << '\n';
        }
        if (const char* value = std::getenv("A")) {
            std::cout << "A=" << value << '\n';
        }
        return EXIT_SUCCESS;
    }

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

    const std::string quoted_test_executable = '"' + GetExecutablePath().string() + '"';

    std::vector<std::string> compound_command_output;
    CallCmd("set \"A=1\" && " + quoted_test_executable + " --call-cmd-probe --DBUG", [&](const std::string& line) {
        compound_command_output.emplace_back(line);
        return false;
    });
    Check(std::ranges::find(compound_command_output, "arg=--DBUG") != compound_command_output.end(), "compound command preserves arguments");
    Check(std::ranges::find(compound_command_output, "A=1") != compound_command_output.end(), "compound command passes the configured environment");

    std::vector<std::string> direct_command_output;
    CallCmd(quoted_test_executable + " --call-cmd-probe --DEBUG", [&](const std::string& line) {
        direct_command_output.emplace_back(line);
        return false;
    });
    Check(std::ranges::find(direct_command_output, "arg=--DEBUG") != direct_command_output.end(), "direct command preserves arguments");

    std::vector<std::string> early_exit_output;
    CallCmd("echo First&&echo Second", [&](const std::string& line) {
        early_exit_output.emplace_back(line);
        return true;
    });
    Check(early_exit_output == std::vector<std::string> { "First" }, "callback still stops output processing early");

    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
