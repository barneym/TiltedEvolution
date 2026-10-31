
#include "Utils/Error.h"
#include "launcher.h"
#include <Windows.h>
#include <combaseapi.h>
#include <base/threading/ThreadUtils.h>

#include "utils/ComUtils.h"
#include "script_extender/SEMemoryBlock.h"

#include <TiltedCore/Platform.hpp>

extern void CoreStubsInit();

extern "C"
{
    __declspec(dllexport) int NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

auto kSystemPreloadDlls = {
    L"\\dinput8.dll",   // < Skyrim early init hook
    L"\\dsound.dll",    // < breaks DSound init in game code
    L"\\nvspcap64.dll", // < Nvidia overlay, needs to be loaded before d3d11 device creation
                        // < X360CE v3 is buggy with COM hooks
    L"\\xinput9_1_0.dll", L"\\xinput1_1.dll", L"\\xinput1_2.dll", L"\\xinput1_3.dll", L"\\xinput1_4.dll", L"\\version.dll"};

static void PreloadSystemDlls()
{
    auto loadSystemDll = [](auto dll)
    {
        wchar_t systemPath[512];
        GetSystemDirectoryW(systemPath, _countof(systemPath));

        wcscat_s(systemPath, dll);

        LoadLibraryW(systemPath);
    };

    for (auto dll : kSystemPreloadDlls)
        loadSystemDll(dll);
}

// Code in the launcher image has no unwind data once the game is mapped over it, so under Wine 11
// a thread naming exception raised there reaches the top of the thread and ends the process.
static LONG WINAPI ThreadNameExceptionHandler(PEXCEPTION_POINTERS apExceptionInfo)
{
    if (apExceptionInfo->ExceptionRecord->ExceptionCode == 0x406D1388)
        return EXCEPTION_CONTINUE_EXECUTION;

    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv)
{
    AddVectoredExceptionHandler(1, ThreadNameExceptionHandler);

    Base::SetCurrentThreadName("MainLauncherThread");

    // memory block for Script Extender reserved as early as we can
    script_extender::SEMemoryBlock b;
    if (!b.Good())
    {
        Die(L"Failed to pre-reserve script extender zone.\nAsk Force!");
        return -1;
    }

    PreloadSystemDlls();
    CoreStubsInit();

    ComScope cs;
    TP_UNUSED(cs);

    auto ret = launcher::StartUp(argc, argv);

    return ret;
}
