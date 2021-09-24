#include "pch.h"
#include <Windows.h>
#include "MinHook.h"
#include <WinInet.h>
#include <fstream>
#include <stdio.h>

using namespace std;

#pragma comment(lib, "libMinHook.x86.lib")

// HttpSendRequestW ㄧ计眖 MSDN 琩
typedef BOOL (WINAPI* HTTPSENDREQUESTW)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD);
HTTPSENDREQUESTW fpHttpSendRequestW = NULL;

// 盢戈癟糶 debug.txt 郎い
void DebugLog(wstring str) {
    wofstream file;
    file.open("C:\\debug.txt", std::wofstream::out | std::wofstream::app);
    if (file) file << str << endl << endl;
    file.close();
}

// 芦э DetourHttpSendRequestW ㄧ计
BOOL WINAPI DetourHttpSendRequestW(HINTERNET hRequest,
    LPCWSTR   lpszHeaders,
    DWORD     dwHeadersLength,
    LPVOID    lpOptional,
    DWORD     dwOptionalLength) {
    // 材把计琌 Request Headerрウ糶郎い
    if (lpszHeaders) {
        DebugLog(lpszHeaders);
    }

    // 材把计硄盽琌 POSTPUT 把计рウ糶郎い
    if (lpOptional) {
        wchar_t  ws[1000];
        swprintf(ws, 1000, L"%hs", (char *)lpOptional);
        DebugLog(ws);
    }

    // ㊣セ HttpSendRequestW
    return fpHttpSendRequestW(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
}

int hook() {
    wchar_t log[256];

    // 1. 眔 wininet.dll  Handle
    HINSTANCE hDLL = LoadLibrary(L"wininet.dll");
    if (!hDLL) {
        DebugLog(L"LoadLibrary wininet.dll failed\n");
        return 1;
    }

    // 2. 眖 wininet.dll т HttpSendRequestW 
    void* HttpSendRequestW = (void*)GetProcAddress(hDLL, "HttpSendRequestW");
    if (!HttpSendRequestW) {
        DebugLog(L"GetProcAddress HttpSendRequestW failed\n");
        return 1;
    }

    // 3. Hook HttpSendRequestW盢ウ芦эΘи﹚竡ㄧ计
    //    ﹚竡ㄧ计い穦р材把计籔材把计糶郎い礛㊣セ HtppSendRequestW
    if (MH_Initialize() != MH_OK){
        DebugLog(L"MH_Initialize failed\n");
        return 1;
    }
    int status = MH_CreateHook(HttpSendRequestW, &DetourHttpSendRequestW, reinterpret_cast<LPVOID*>(&fpHttpSendRequestW));
    if (status != MH_OK) {
        swprintf_s(log, L"MH_CreateHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    // 4. 币ノ Hook
    status = MH_EnableHook(HttpSendRequestW);
    if (status != MH_OK) {
        swprintf_s(log, L"MH_EnableHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
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