#include "log/logger.hpp" // 必须包含、用以生成 lib

static BOOL APIENTRY DllMain([[maybe_unused]] HMODULE hModule, DWORD ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH: break;
    }
    return TRUE;
}
