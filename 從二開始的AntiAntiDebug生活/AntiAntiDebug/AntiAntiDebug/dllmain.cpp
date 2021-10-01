#include "pch.h"
#include <Windows.h>
#include "MinHook.h"

#pragma comment(lib, "libMinHook.x64.lib")

// IsDebuggerPresent 的函數原型，可以從 MSDN 查到
typedef BOOL(WINAPI* ISDEBUGGERPRESENT)();
ISDEBUGGERPRESENT fpIsDebuggerPresent = NULL;

// 竄改 IsDebuggerPresent 的函數
// 直接 return FALSE，代表沒有 Debugger
BOOL WINAPI DetourIsDebuggerPresent() {
    return FALSE;
}

int hook() {
    // Hook IsDebuggerPresent，將它竄改成我們自己定義的函數。
    if (MH_Initialize() != MH_OK) {
        return 1;
    }
    MH_STATUS status = MH_CreateHook(IsDebuggerPresent, &DetourIsDebuggerPresent, reinterpret_cast<LPVOID*>(&fpIsDebuggerPresent));
    if (status != MH_OK) {
        return 1;
    }

    // 啟用 Hook
    status = MH_EnableHook(IsDebuggerPresent);
    if (status != MH_OK) {
        return 1;
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        hook();
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}