# 【Day 09】Hook 的奇妙冒險 - Ring3 Hook

## 環境
* Windows 10 21H1
* Visual Studio 2019
* x64dbg Aug 2 2020, 13:56:14

## 簡介
Hook 這項技巧用於各種作業系統，可以達到攔截程式的效果。它也同時是紅隊與藍隊常用的手段，畢竟「攻防本一體」，紅隊希望能利用 Hook 達到獲得機敏資訊或繞過驗證和偵測等等效果，而籃隊則是為了可以有效監控特定 Process。

原理上也十分淺顯易懂，基本上就是把原本程式要執行的部分改掉，把 Instruction Pointer 跳到自己定義的程式段。那具體上是要改成什麼，要怎麼把自己的程式放到目標 Process，程式裡面又要做些什麼，這些在下面會說明。

既然出現了 Hook 這個技巧，當然也會有偵測 Hook 的技術，而有了偵測 Hook 的技術，就也會有繞過偵測 Hook 技術的 Hook 技巧，如此循環...

## Ring3 Hook vs Ring0 Hook
![](https://i.imgur.com/rQzo4nc.png)

所謂 Ring3(User Mode) Hook，就是在不會碰到 Kernel API 的條件下所完成的 Hook。

根據上圖，以 Hook CreateFile 為例，在呼叫此函數時，其實是使用 Kernel32.dll 所 Export 的函數 CreateFile。Ring3 Hook 就可以把 CreateFile 修改成 `jmp <address>`，而 address 就是我們所要更換執行的程式。

繼續 Hook CreateFile 的例子，Ring0(Kernel Mode) Hook 則是要再往後一點。在呼叫 Kernel32.dll 中的 CreateFile 後，其中又會呼叫 Ntdll.dll 的 NtWriteFile，再往下走會去 SSDT(System Services Descriptor Table) 查表並執行同名的 Kernel API - NtWriteFile。因為 SSDT 有存放函數地址，因此 Ring0 Hook 可以透過修改 SSDT 達到 Hook 效果。

其實 Ring0 Hook 還不只可以 Hook SSDT，還有許多手法可以達到相同功能。但是在 Windows XP、Windows Server 2003 版本後，因為 PatchGuard 的緣故，在一般的情況下無法做 Kernel Patch。而之後陸續出現繞過 PatchGuard 的方法，以及微軟的修補，與再度的繞過就又是另一個故事了。

這篇文主要還是針對 Ring3 Hook 講解，介紹我覺得不錯的 Hook Library，並說明其實作原理。

## Hook Library
以下找了幾個開源的 Hook Library，到了最近都還有在更新，自己試了也覺得不錯。
* [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook)
* [gdabah/distormx](https://github.com/gdabah/distormx)
* [Zeex/subhook](https://github.com/Zeex/subhook)

這次的 POC 將會使用 MinHook 來實作簡單的 Hook。

## 實作
### 實作流程
1. 定義要 Hook 的函數，因為等等要把它改成自己的函數，所以最好參數可以對應原本的函數。這次的目標是 MessageBoxW，對這個 API 不熟的可以看看 [MSDN: MessageBoxW](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-messageboxw)。
2. 將目標函數的資訊記下來，做好 Hook 的前置作業。
3. 啟用 Hook，將目標函數前幾 Byte 改掉，jmp 到自己定義的函數。
4. 停用 Hook，把目標函數改回來

### POC
程式專案放在我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Hook%E7%9A%84%E5%A5%87%E5%A6%99%E5%86%92%E9%9A%AA/MinhookExample/)。
```cpp=
#include <Windows.h>
#include "MinHook.h"

#if defined _M_X64
#pragma comment(lib, "libMinHook.x64.lib")
#elif defined _M_IX86
#pragma comment(lib, "libMinHook.x86.lib")
#endif

// 1. 定義要 Hook 的函數，因為等等要把它改成自己的函數，所以最好參數可以對應原本的函數。
typedef int (WINAPI* MESSAGEBOXW)(HWND, LPCWSTR, LPCWSTR, UINT);
MESSAGEBOXW fpMessageBoxW = NULL;

int WINAPI DetourMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    return fpMessageBoxW(hWnd, L"Hooked\n", lpCaption, uType);
}

int main() {
    // 2. 將目標函數的資訊記下來，做好 Hook 的前置作業。
    if (MH_Initialize() != MH_OK) {
        return 1;
    }
    if (MH_CreateHook(&MessageBoxW, &DetourMessageBoxW, reinterpret_cast<LPVOID*>(&fpMessageBoxW)) != MH_OK) {
        return 1;
    }

    // 3. 啟用 Hook，將目標函數前幾 Byte 改掉，jmp 到自己定義的函數。
    if (MH_EnableHook(&MessageBoxW) != MH_OK)
    {
        return 1;
    }

    // 測試目標函數被改掉後有沒有變成自己定義的函數，這邊應該要跳出 Hooked
    MessageBoxW(NULL, L"Not hooked\n", L"MinHook Example", MB_OK);

    // 4. 停用 Hook，把目標函數改回來
    if (MH_DisableHook(&MessageBoxW) != MH_OK) {
        return 1;
    }

    // 測試目標函數有沒有被改回來，這邊應該要跳出 Not hooked
    MessageBoxW(NULL, L"Not hooked\n", L"MinHook Example", MB_OK);

    if (MH_Uninitialize() != MH_OK) {
        return 1;
    }
    return 0;
}
```

### 開 x64dbg 看看
基本上在 MSDN 最下面都會說明該函數適用於什麼環境，存在哪個 Library、DLL 中，以下是 MessageBoxW 的資訊
![](https://i.imgur.com/R1bj2Fm.png)

因此可以透過 x64dbg 找到函數在 Process 中的位址
![](https://i.imgur.com/BfV1ReS.png)

這是在 Hook 之前的 MessageBoxW
![](https://i.imgur.com/wxgpHlk.png)

在執行 MH_EnableHook 之後，可以看到第一個 Instruction 變成 jmp 了，這邊注意被蓋掉的 Instruction 是 `sub rsp, 38; xor r11d, r11d;`
![](https://i.imgur.com/s0VAm0t.png)

到了這邊大家可能會有個疑問，如果現在原本的 MessageBoxW 的前幾個 Byte 被改掉了，難道在 DetourMessageBoxW 中呼叫 fpMessageBoxW 的時候不會出錯嗎?
這部分就繼續往下追，下圖是在 DetourMessageBoxW 準備呼叫 fpMessageBoxW 的狀況
![](https://i.imgur.com/mNIrw92.png)

而 fpMessageBoxW 中就有把原本被蓋掉的部分寫到裡面，也就是 `sub rsp, 38; xor r11d, r11d;`，最後後才跳回原本的 MessageBoxW 繼續執行。
![](https://i.imgur.com/UUc4EXp.png)

最後在執行 MH_DisableHook 又把函數改回來了，以上就是 Hook 的基本原理。

## 其他
如果不想使用 Hook Library，而是想要自己實作 Hook，可以去翻翻上面推薦的開源程式碼。最基本的原理就是用 [VirtualProtect](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualprotect) 把函數的權限改成可寫，因為一般因為 DEP/NX 保護無法直接寫 Text Section。再來用 [VirtualAlloc](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc) 申請一塊記憶體將自己的程式寫入。然而實作方法不只一種，上面說的只是其中一種，有時會為了不容易被偵測，甚至不使用 VirtualAlloc，而是使用 Code Caves。

接下來會介紹如何偵測 Hook，還有如何繞過偵測，有種像是玩捉迷藏的感覺。

## 參考資料
* [GuideHacking: Hook 原理](https://guidedhacking.com/threads/how-to-hook-functions-code-detouring-guide.14185/)
* [Hook SSDT](https://www.twblogs.net/a/5b82ce0b2b717766a1e9feb9)
* [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook)
* [gdabah/distormx](https://github.com/gdabah/distormx)
* [Zeex/subhook](https://github.com/Zeex/subhook)
