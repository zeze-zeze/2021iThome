# 【Day 11】卑鄙源之 Hook (上) - 偵測 Hook

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在 [【Day 10】穿過 IE 的巴巴 - Hook IE 竊取明文密碼](https://ithelp.ithome.com.tw/articles/10271645)講了 Hook 的紅隊利用，從 IE 竊取明文密碼。可能有些人已經開始感到焦慮了，難道我們沒有方法抵擋攻擊嗎，所以這篇就介紹藍隊的防守方法。

從之前的 [【Day 09】Hook 的奇妙冒險 - Ring3 Hook](https://ithelp.ithome.com.tw/articles/10270919) 可以知道，Hook 其實就是改掉原本的 Function，控制程式的進行，改成執行我們自己定義的 Function。基於這一點，我們能怎麼偵測 Hook 呢？

這篇文章會拿之前講的 Hook IE 的偷取明文連線資訊的 POC 當作目標，實作偵測 Hook 的程式。

## 原理
Hook 的本質就是 Patch，它必須改掉原本的程式來達到控制程式的目的。根據這點，我們很容易就能想到一個偵測的方法，就是比對檔案與記憶體的內容。

因為程式在載入 Image 時，並不是直接執行檔案，而是把檔案內容複製到記憶體中執行。也就是說，舉 Hook IE 中的 wininet.dll 的 SendHttpRequestW 為例，我實際上改的是記憶體中的 SendHttpRequestW，而 wininet.dll 檔案本身並沒有被改動到。

因此只要比對沒有被改動到的檔案，與不確定有沒有被改動的記憶體，在大部分的情況下，如果兩者相同，就代表沒有被 Hook。

雖然原理簡單，卻有不少魔鬼小細節，因此這個主題會分兩篇講解。這篇處理記憶體，下一篇處理檔案。

## 從記憶體中找目標 Module
目前已知目標 Process 是 iexplorer.exe，目標 Module 是 WININET.DLL。首先可以看一下 [EnumProcessModules](https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodules) 這個函數。其中 lphModule 放著所有 Module 的陣列，我們的目標 wininet.dll 就在這裡面。
```
BOOL EnumProcessModules(
  HANDLE  hProcess,
  HMODULE *lphModule,
  DWORD   cb,
  LPDWORD lpcbNeeded
);
```

那要怎麼知道目前的這個 Module 是不是我們的目標呢？可以使用 [GetModuleFileNameExW](https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulefilenameexw) 這個函數。其中 lpFilename 就是 Module 的檔案路徑，只要比對它有沒有包含 wininet.dll 字串就可以知道它是不是我們的目標。
```
DWORD GetModuleFileNameExW(
  HANDLE  hProcess,
  HMODULE hModule,
  LPWSTR  lpFilename,
  DWORD   nSize
);
```

取得 wininet.dll 這個 Module 的 Handle 後，可以使用 [GetModuleInformation ](https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmoduleinformation) 取得有關這個 Module 的資訊。其中 Module 的 Base Address 就在 lpmodinfo 裡面。lpmodinfo 是一個 [MODULEINFO](https://docs.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-moduleinfo) 結構。
```
BOOL GetModuleInformation(
  HANDLE       hProcess,
  HMODULE      hModule,
  LPMODULEINFO lpmodinfo,
  DWORD        cb
);
```


## 實作
### 實作流程
1. 開啟目標 Process (iexplore.exe 的 pid)
2. 用 EnumProcessModules 取得所有的 Module Handle
3. 用 GetModuleFileNameEx 取得目前的 Module Name
4. 判斷是不是目標 (WININET)，是的話就記錄下來
5. 用 GetModuleInformation 取得 Module 資訊

### POC
程式的專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%8D%91%E9%84%99%E6%BA%90%E4%B9%8BHook/FindModule)
```cpp=
#include <windows.h>
#include <string>
#include <psapi.h>

int main(int argc, char* argv[]) {
    // 1. 開啟目標 Process (iexplorer.exe 的 pid)
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 1912);
    if (!hProcess) {
        printf("OpenProcess failed: error %d\n", GetLastError());
        return 1;
    }

    // 2. 用 EnumProcessModules 取得所有的 Module Handle
    HMODULE hMods[1024], hModule = NULL;
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (int i = 0; i < (int)(cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModPathName[MAX_PATH] = { 0 };

            // 3. 用 GetModuleFileNameEx 取得目前的 Module Name
            if (GetModuleFileNameEx(hProcess, hMods[i], szModPathName, sizeof(szModPathName) / sizeof(TCHAR))) {
                printf("%ls\n", szModPathName);

                // 4. 判斷是不是目標 (WININET)，是的話就記錄下來
                std::wstring sMod = szModPathName;
                if (sMod.find(L"WININET") != std::string::npos) {
                    hModule = hMods[i];
                }
            }
            else {
                printf("GetModuleFileNameEx failed: error %d\n", GetLastError());
                return NULL;
            }
        }
    }
    else {
        printf("EnumProcessModulesEx failed: error %d\n", GetLastError());
        return 1;
    }
    if (hModule == NULL) {
        printf("Cannot find target module\n");
        return 1;
    }
    printf("\n\nWININET.dll handle: %p\n", hModule);
    
    // 5. 用 GetModuleInformation 取得 Module 資訊
    MODULEINFO lpmodinfo;
    if (!GetModuleInformation(hProcess, hModule, &lpmodinfo, sizeof(MODULEINFO))) {
        printf("GetModuleInformation failed: error %d\n", GetLastError());
        return 1;
    }
    return 0;
}
```

### 實際測試
記得把 POC 的 pid 改成自己環境測試的 iexplore.exe 的 pid，然後要編成 x86 的執行檔，因為目標 Process 是 32-bit。程式會列舉所有 Image，雖然使用的函數不同，但這也算是完成 Sysinternals 中的 Listdlls.exe 的最基本功能。
```
# FindModule.exe
C:\Program Files (x86)\Internet Explorer\IEXPLORE.EXE
C:\WINDOWS\SYSTEM32\ntdll.dll
C:\WINDOWS\System32\KERNEL32.DLL
C:\WINDOWS\System32\KERNELBASE.dll
C:\WINDOWS\SYSTEM32\apphelp.dll
C:\WINDOWS\System32\USER32.dll
C:\WINDOWS\System32\win32u.dll
C:\WINDOWS\System32\GDI32.dll
C:\WINDOWS\System32\gdi32full.dll
C:\WINDOWS\System32\msvcp_win.dll
C:\WINDOWS\System32\ucrtbase.dll
C:\WINDOWS\System32\msvcrt.dll
C:\WINDOWS\System32\ADVAPI32.dll
C:\WINDOWS\System32\sechost.dll
C:\WINDOWS\System32\RPCRT4.dll
C:\WINDOWS\System32\combase.dll
C:\WINDOWS\SYSTEM32\iertutil.dll
C:\WINDOWS\System32\shcore.dll
C:\WINDOWS\System32\IMM32.DLL
C:\WINDOWS\System32\bcryptPrimitives.dll
C:\WINDOWS\SYSTEM32\msIso.dll
C:\WINDOWS\SYSTEM32\kernel.appcore.dll
C:\WINDOWS\SYSTEM32\IEFRAME.dll
C:\WINDOWS\System32\SHLWAPI.dll
C:\WINDOWS\System32\ole32.dll
C:\WINDOWS\System32\OLEAUT32.dll
C:\WINDOWS\System32\SHELL32.dll
C:\WINDOWS\SYSTEM32\USERENV.dll
C:\WINDOWS\SYSTEM32\VERSION.dll
C:\WINDOWS\SYSTEM32\NETAPI32.dll
C:\WINDOWS\SYSTEM32\WINHTTP.dll
C:\WINDOWS\SYSTEM32\WKSCLI.DLL
C:\WINDOWS\SYSTEM32\NETUTILS.DLL
C:\WINDOWS\WinSxS\x86_microsoft.windows.common-controls_6595b64144ccf1df_6.0.19041.1110_none_a8625c1886757984\comctl32.dll
C:\Program Files (x86)\Internet Explorer\IEShims.dll
C:\WINDOWS\System32\comdlg32.dll
C:\WINDOWS\system32\uxtheme.dll
C:\WINDOWS\SYSTEM32\MSHTML.dll
C:\WINDOWS\SYSTEM32\SspiCli.dll
C:\WINDOWS\SYSTEM32\powrprof.dll
C:\WINDOWS\SYSTEM32\UMPDC.dll
C:\WINDOWS\SYSTEM32\CRYPTBASE.DLL
C:\WINDOWS\SYSTEM32\urlmon.dll
C:\WINDOWS\SYSTEM32\srvcli.dll
C:\WINDOWS\System32\clbcatq.dll
C:\WINDOWS\system32\windows.storage.dll
C:\WINDOWS\system32\Wldp.dll
C:\WINDOWS\SYSTEM32\WININET.dll
C:\WINDOWS\system32\profapi.dll
C:\WINDOWS\System32\WS2_32.dll
C:\WINDOWS\SYSTEM32\ondemandconnroutehelper.dll
C:\WINDOWS\system32\mswsock.dll
C:\WINDOWS\SYSTEM32\IPHLPAPI.DLL
C:\WINDOWS\SYSTEM32\WINNSI.DLL
C:\WINDOWS\System32\NSI.dll
C:\Windows\System32\ieproxy.dll
C:\WINDOWS\System32\MSCTF.dll
C:\WINDOWS\SYSTEM32\IEUI.dll
C:\WINDOWS\SYSTEM32\d2d1.dll
C:\WINDOWS\SYSTEM32\DWrite.dll
C:\WINDOWS\SYSTEM32\dxgi.dll
C:\WINDOWS\SYSTEM32\ieapfltr.dll
C:\WINDOWS\SYSTEM32\CRYPTSP.dll
C:\WINDOWS\System32\bcrypt.dll
C:\WINDOWS\SYSTEM32\d3d11.dll
C:\WINDOWS\System32\DriverStore\FileRepository\iigd_dch.inf_amd64_7afabf937b5bd7bd\igd10iumd32.dll
C:\WINDOWS\System32\CRYPT32.dll
C:\WINDOWS\SYSTEM32\ncrypt.dll
C:\WINDOWS\SYSTEM32\NTASN1.dll
C:\WINDOWS\System32\DriverStore\FileRepository\iigd_dch.inf_amd64_7afabf937b5bd7bd\igdgmm32.dll
C:\WINDOWS\System32\DriverStore\FileRepository\iigd_dch.inf_amd64_7afabf937b5bd7bd\igc32.dll
C:\WINDOWS\SYSTEM32\dxcore.dll
C:\WINDOWS\System32\cfgmgr32.dll
C:\WINDOWS\system32\dataexchange.dll
C:\WINDOWS\system32\dcomp.dll
C:\WINDOWS\system32\twinapi.appcore.dll
C:\WINDOWS\SYSTEM32\TextShaping.dll
C:\WINDOWS\SYSTEM32\srpapi.dll
C:\WINDOWS\SYSTEM32\ninput.dll
C:\WINDOWS\system32\mlang.dll
C:\Windows\System32\jscript9.dll
C:\WINDOWS\SYSTEM32\dwmapi.dll
C:\WINDOWS\SYSTEM32\sxs.dll
C:\WINDOWS\System32\IDStore.dll
C:\WINDOWS\System32\SAMLIB.dll
C:\WINDOWS\System32\PROPSYS.dll
C:\WINDOWS\SYSTEM32\tbs.dll
C:\WINDOWS\System32\imagehlp.dll
C:\WINDOWS\system32\msimtf.dll
C:\WINDOWS\system32\directmanipulation.dll
C:\Windows\System32\vaultcli.dll
C:\WINDOWS\SYSTEM32\wintypes.dll
C:\WINDOWS\SYSTEM32\Secur32.dll
C:\WINDOWS\SYSTEM32\textinputframework.dll
C:\WINDOWS\System32\CoreMessaging.dll
C:\WINDOWS\System32\CoreUIComponents.dll
C:\WINDOWS\SYSTEM32\ntmarta.dll
C:\Windows\System32\OneCoreCommonProxyStub.dll


WININET.dll handle: 71280000
```


## 參考資料
* [HookHunter](https://github.com/mike1k/HookHunter)
