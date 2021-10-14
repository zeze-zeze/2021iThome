# 【Day 17】從二開始的 Anti-Anti-Debug 生活 - Anti-Anti-Debug

## 環境
* Windows 10 21H1
* x64dbg Aug 2 2020, 13:56:14
* Visual Studio 2019
* IDA 7.5

## 前情提要
上上篇[【Day 15】從零開始的 Debug 生活 - Debugger 原理](https://ithelp.ithome.com.tw/articles/10274999)介紹了 Debugger 的基本運作原理，上一篇[【Day 16】從一開始的 Anti-Debug 生活 - Anti-Debug
](https://ithelp.ithome.com.tw/articles/10275614)介紹針對 Debugger 運作原理的 Anti-Debug 的技巧，所以這一篇當然就要來 Anti-Anti-Debug，這一篇將會是這個 Debugger 系列的最後一篇。

這篇在 64-bit 環境下使用 x64dbg 講解，會介紹一些常見的 Anti-Anti-Debug 手法，繞過上一篇寫的 Anti-Debug 的 POC。

## Anti-Anti-Debug
常見的 Anti-Anti-Debug 手法大致有以下幾種
* Patch
* 在 Debugger 中繞過
* Hook
* 寫 Debugger Plugin

### Patch
把做 Anti-Debug 的部分程式碼改掉，例如改成 `nop`，機械碼為 `90`，或者是把條件判斷的 `je` 改成 `jmp` 等等。

以上一篇的 Debug Flag 的 Anti-Debug 的 POC 為例，用 IDA 反組譯出來如下。可以注意到 `call IsDebuggerPresent` 之後有個 `test eax, eax`，這是在判斷 IsDebuggerPresent 的回傳值是否為 0，如果是 0 就會跳到 `loc_140117AE`，然後跳出 `Not Detect` 的訊息框。
![](https://i.imgur.com/jXNhAae.png)

所以如果不希望程式偵測到我們有在使用 Debugger，也就是不想讓 `Detect` 的訊息框跳出，就只要把 `jz short loc_1400117AE` 的 `jz` 改成 `jmp` 就可以了。改完後再拿去 x64dbg 執行一次，會發現跳出 `Not Detect` 的訊息框。
IDA => Edit => Patch program => Assemble
![](https://i.imgur.com/YjTOFIi.png)

### 在 Debugger 中繞過
直接在 Debugger 中影響程式流程，可以用 Patch，也可以改暫存器或記憶體，總之就是想辦法不被 Anti-Debug 影響到就對了。

直接用 x64dbg 執行，並下斷點在 main。
![](https://i.imgur.com/RD83F9O.png)

現在按空白鍵，把 `je` 改成 `jmp`，就可以無視 IsDebuggerPresent 的回傳值直接繞過 Anti-Debug。
![](https://i.imgur.com/derKg3z.png)


### Hook
在[【Day 09】Hook 的奇妙冒險 - Ring3 Hook](https://ithelp.ithome.com.tw/articles/10270919) 有介紹過 Hook 的原理與實作，我們也可以使用這個技巧來實作 Anti-Debug。以 IsDebuggerPresent 為例，當回傳值為 0(FALSE) 時代表沒有 Debugger，否則就是正在 Debug。因此我們可以藉由 Hook IsDebuggerPresent，讓它無條件回傳 0，這樣就可以繞過 Anti-Debug 的檢查。

#### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%BE%9E%E4%BA%8C%E9%96%8B%E5%A7%8B%E7%9A%84AntiAntiDebug%E7%94%9F%E6%B4%BB/AntiAntiDebug)
```cpp=
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
```

### 寫 Debugger Plugin
自己寫 Debugger Plugin 的好處是彈性較大，不只可以 Patch、Hook，還可以改變 Debugger 處理 Exception 的方式。

這邊我推薦一個 x64dbg 的 Anti-Anti-Debugger 工具 [ScyllaHide](https://github.com/x64dbg/ScyllaHide)，功能包含 Hook Function、Patch PEB、檢查暫存器、檢查 Exception 等等，把大多現有的 Anti-Debug 技巧都繞過了。而且到了最近都還有在更新，下圖為目前有的功能，
![](https://i.imgur.com/vNsIX92.png)



安裝方法就下載 [Release](https://github.com/x64dbg/ScyllaHide/releases/tag/snapshot-2021-08-23_13-27-50) 的 7z 檔案，把裡面的 HookLibraryx64.dll、scylla_hide.ini、ScyllaHideX64DBGPlugin.dp64 複製到 x64/plugins 中，x32 的也一樣。

使用方法也非常簡單，安裝完就已經啟動了基本的模式，其他還有比較進階的模式就看個人需求再選擇。拿 IsDebuggerPresent 的例子，在 x64dbg 執行檔案就不會偵測到有 Debugger，因為 ScyllaHide 已經把 Debug Flag 改掉了。

## 參考資料
* [Anti-Debug](https://anti-debug.checkpoint.com/)
* [ScyllaHide](https://github.com/x64dbg/ScyllaHide)

