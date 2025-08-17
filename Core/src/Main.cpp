#include "SingletonData.hpp"

#include "Function.h"
#include "ThreadPool.hpp"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    SCOPED_TIMER("main");
    SINGLE_DATA.VM = ProcessArguments(argc, argv);


    utils::ThreadPool pool(CPU_NUM);
    std::vector<std::function<void()>> tasks = {
        []() { CallCmd("ping www.baidu.com"); },
        []() { CallCmd("ping www.bing.com"); },
        []() { CallCmd("ping www.bilibili.com"); },
        []() { CallCmd("ping 127.0.0.1"); }
    };
    for (auto& task : tasks) {
        pool.enqueue(task);
    }
    pool.wait_for_completion();

    _Logging_::Logger::get_instance().ShutDown();
    return 0;
}
