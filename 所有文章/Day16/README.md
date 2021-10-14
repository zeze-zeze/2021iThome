# 【Day 16】從一開始的 Anti-Debug 生活 - Anti-Debug

## 環境
* Windows 10 21H1
* x64dbg Aug 2 2020, 13:56:14
* Visual Studio 2019

## 前情提要
上一篇[【Day 15】從零開始的 Debug 生活 - Debugger 原理](https://ithelp.ithome.com.tw/articles/10274999)提到一些 Debugger 的實作原理，了解斷點、Flag 設定的實作方式，認識 Debugger 的基本行為。

這一篇會說明一般常見的 Anti-Debug 的方法，不過方法其實非常多，為了保持這個系列的連貫性，這一篇主要針對上一篇提到的 Debugger 基本行為實作 Anti-Debug。

下面介紹與實作的部分都是以 64-bit 的環境為前提，但是 32-bit 原理其實差不多。另外不同 Debugger 因為實作方式不同，導致 Anti-Debug 不一定能在每個 Debugger 上成功，這篇以 x64dbg 為主。

## Anti-Debug
### Debug Flag
#### 原理
上一篇在講 Debug Flag 時，曾說過 Debug Flag 是在 [PEB](https://www.nirsoft.net/kernel_struct/vista/PEB.html) 結構中的一個成員 `BeingDebugged`。可以透過檢查這個 Debug Flag 的值確認目前有沒有被 Debug。

這部分的實作方法很豐富，既可以使用 Windows API，也可以自己找出 PEB 結構確認。這篇就使用最簡單的 [IsDebuggerPresent](https://docs.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-isdebuggerpresent)。

#### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%BE%9E%E4%B8%80%E9%96%8B%E5%A7%8B%E7%9A%84AntiDebug%E7%94%9F%E6%B4%BB/AntiDebugDebugFlag)。
```cpp=
#include<Windows.h>

int main(int argc, char* argv[]) {
    if (IsDebuggerPresent()) {
        MessageBoxW(0, L"Detect", L"Debugger", 0);
    }
    else {
        MessageBoxW(0, L"Not Detect", L"Debugger", 0);
    }
}
```

#### 實際測試
正常狀況下執行，會跳出 Not Detect 的訊息框；如果使用 Debugger，則會跳出 Detect 的訊息框。當然如果在 Debugger 把 PEB 結構的 BeingDebugged 改成 0 就不會偵測到 Debugger。

### Software Breakpoint
#### 原理
上一篇說明 Software Breakpoint 時，有說 Debugger 會把下斷點的位址改成 `int3`，然後 Debugger 會處理產生的 Exception。

同個原理可以也被用在 Step-Over，在 x64dbg 中按下 F8 可以直接把呼叫的函數執行完並停在 `call` 的下一個指令。這個實作原理就是把呼叫的函數的 Return Address 改成 `int3`。

所以如果我們在程式中定義一個函數，並在函數檢查 Return Address 是不是 `int3`，如果是就改掉，讓它無法觸發 Exception。

#### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%BE%9E%E4%B8%80%E9%96%8B%E5%A7%8B%E7%9A%84AntiDebug%E7%94%9F%E6%B4%BB/AntiDebugSoftwareBreakpoint)。
```cpp=
#include <intrin.h>
#include <Windows.h>
#pragma intrinsic(_ReturnAddress)

void PatchInt3() {
    PVOID pRetAddress = _ReturnAddress();

    // 如果 Return Address 是 int3 (0xcc)，就改掉
    if (*(PBYTE)pRetAddress == 0xCC) {
        DWORD dwOldProtect;
        if (VirtualProtect(pRetAddress, 1, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            // 這邊是填成 nop(0x90)，在這個 POC 不會  Crash。
            // 但是最好還是改成原本 Return Address 的值，否則可能會 Crash
            *(PBYTE)pRetAddress = 0x90;
            VirtualProtect(pRetAddress, 1, dwOldProtect, &dwOldProtect);
        }
    }
}
int main(int argc, char* argv[]) {
    PatchInt3();
    MessageBoxW(0, L"You cannot keep debugging", L"Give up", 0);
}
```

#### 實際測試
開 Debugger 到要執行 PatchInt3 之前，按下 F8 Step-Over，會直接跳出 You cannot keep debugging 的訊息框並且直接繼續執行到結束，因為斷點已經被我們的程式取消了。

### Trap Flag
#### 原理
上一篇介紹 Trap Flag 有提到它是 CPU 的其中一個 Flag，在 1(set) 時會觸發 EXCEPTION_SINGLE_STEP，而 Debugger 會去處理這個 Exception。

因此我們可以利用這一點，故意設 Trap Flag 為 1(set)，如果是在 Debugger 中，會因為 Exception 被 Debugger 處理而不會 Crash；但是如果程式是被正常的執行，則會因為產生的 Exception 而 Crash。

#### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%BE%9E%E4%B8%80%E9%96%8B%E5%A7%8B%E7%9A%84AntiDebug%E7%94%9F%E6%B4%BB/TrapFlagAntiDebug)。
* SetTrapFlag.asm
```=
.CODE
SetTrapFlag PROC
    ; 把 Flag 們丟到 Stack 上
    pushf

    ; 設定 Trap Flag，位置在第 9 個 bit，所以是 0x100
    or dword ptr [rsp], 100h

    ; 把新的 Flag 從 Stack 拿給 Flag 們
    popf
    nop
    ret
SetTrapFlag ENDP
END
```
* TrapFlagAntiDebug.cpp
```cpp=
#include <Windows.h>

extern "C" void SetTrapFlag();

int main(int argc, char* argv[]) {
    __try {
        // 如果是在 Debugger 中執行這個 Function，
        // 觸發的 Exception 卻沒有被自己定義的意外處理方式處理，
        // 表示這個 Exception 已經被 Debugger 處理了，也就代表目前正在被 Debug
        SetTrapFlag();
        MessageBoxW(0, L"Detect", L"Debugger", 0);
    }
    __except (GetExceptionCode() == EXCEPTION_SINGLE_STEP
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_EXECUTION) {
        MessageBoxW(0, L"Not Detect", L"Debugger", 0);
    }
}
```

#### 實際測試
正常執行的情況下，會跳出 Not Detect 的訊息框。如果想要弄出 Detect 的訊息框，就開 Debugger，然後要一步一步執行，所以得按 F7 跟進 SetTrapFlag 函數。

### Hardware Breakpoint
#### 原理
上一篇介紹 Hardware Breakpoint 有提到它是透過設定 DR0~DR3 達到斷點的效果，所以只要在程式中檢查這四個暫存器有沒有被使用就可以偵測 Hardware Breakpoint。

具體實作方式可以使用 [GetThreadContext](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext) 函數，取得的 [Context 結構](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context)中就有暫存器的資訊。

#### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%BE%9E%E4%B8%80%E9%96%8B%E5%A7%8B%E7%9A%84AntiDebug%E7%94%9F%E6%B4%BB/AntiDebugHardwareBreakpoint)。
```cpp=
#include <Windows.h>

int main(int argc, char* argv[]) {
    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // 取得 Context 結構
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;

    // 確認 DR0~DR3 有沒有被設定，有非 0 值代表有 Hardware Breakpoint
    if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) {
        MessageBoxW(0, L"Detect", L"Hardware Breakpoint", 0);
    }
    else {
        MessageBoxW(0, L"Not Detect", L"Hardware Breakpoint", 0);
    }
}
```

#### 實際測試
正常執行會跳出 Not Detect 的訊息框；開啟 Debugger 設定硬體斷點後執行，會跳出 Detect 的訊息框。

## 參考資料
* [external assembly file in visual studio](https://stackoverflow.com/questions/33751509/external-assembly-file-in-visual-studio)
* [x64dbg: int3 處理](https://github.com/x64dbg/x64dbg/issues/1614)
* [Anti-Debug](https://anti-debug.checkpoint.com/)
* [HackOvert/AntiDBG](https://github.com/HackOvert/AntiDBG)
* [DR0~DR7](https://www.itread01.com/content/1549387445.html)
