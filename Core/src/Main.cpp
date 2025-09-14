#include "Function.h"
#include "SingletonData.hpp"

#include "CgnsCore.h"

#include <fstream>
#include <set>

#include "meojson\json.hpp"

void Test()
{
    constexpr std::string filename = "info.json";

    if (std::filesystem::exists(filename)) {
        std::fstream file(filename, std::ios::in);
        if (!file.is_open()) {
            LOG_ERROR("open [{}] faild.", filename);
            return;
        }

        std::string json_string = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        file.close();

        auto ret = json::parse(json_string);
        if (!ret) {
            LOG_ERROR("parse faild.");
            return;
        }
        json::value& value = (*ret);

        constexpr std::array new_elementSets = { "new_input", "new_output" };
        auto& steps = value["steps"].as_array();
        for (auto& step : steps) {
            // elementSets
            auto& elementSets = step["elementSets"].as_array();
            elementSets.emplace_back(new_elementSets[0]);
            elementSets.emplace_back(new_elementSets[1]);
            LOG_INFO(elementSets.to_string());

            // variables
            auto& variables = step["variables"].as_object();

            std::set velocity = { "velocity1", "velocity2", "velocity3" };
            variables.emplace("velocity", velocity);

            LOG_INFO(variables.to_string());
        }

        std::fstream outFile("info.json", std::ios::out);
        if (outFile.is_open()) {
            outFile << ret->to_string();
            outFile.close();
        }
        else {
            LOG_ERROR("error opening file for writing");
        }
    }
    else {
        static constexpr auto json_text = R"({
    "steps": [
        {
            "name": "",
            "type": "",
            "origin_frame_num": "",
            "frames": [],
            "ips": [],
            "layers": [],
            "elementSets": [],
            "variables": {}
        }
    ]
})";

        auto ret = json::parse(json_text);
        if (!ret) {
            LOG_ERROR("parse faild.");
            return;
        }
        json::value& value = (*ret);

        auto& steps = value["steps"].as_array();
        for (auto& step : steps) {
            step["name"] = "Step-1";
            step["type"] = "fluid";
            step["origin_frame_num"] = "1";

            step["frames"].emplace("0");
            step["ips"].emplace("NULL");
            step["layers"].emplace("NULL");

            step["elementSets"].emplace("input");
            step["elementSets"].emplace("output");
        }

        std::fstream outFile("info.json", std::ios::out);
        if (outFile.is_open()) {
            outFile << ret->to_string();
            outFile.close();
        }
        else {
            LOG_ERROR("error opening file for writing");
        }
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main");
    SINGLE_DATA.VM = ProcessArguments(argc, argv);

    Test();

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}

