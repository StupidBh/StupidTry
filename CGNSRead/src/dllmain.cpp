// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "CGNSReadFramework.h"

#include "CgnsCore.h"
#include "log/logger.hpp"

BOOL APIENTRY DllMain([[maybe_unused]] HMODULE hModule, DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH: break;
    }
    return TRUE;
}

void Test()
{
    int cg_file_id = -1;
    cg_open("D:\\SomeThingCode\\tutorial1-adv.cgns", CG_MODE_READ, &cg_file_id);

    float cg_file_version = 0.F;
    cg_version(cg_file_id, &cg_file_version);

    LOG_INFO("CGNS-v{:.2f}", cg_file_version);
    _Logging_::Logger::get_instance().ShutDown();
}
