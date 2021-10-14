# 【Day 23】為美好的 Windows 獻上 ETW - Event Tracing for Windows

## 環境
* Windows 10 21H1

## ETW 介紹
### 歷史
ETW (Event Tracing for Windows) 在 Windows 2000 引入，從這時候起，OS Kernel 和 Service 都會通過 ETW 記錄。Windows Vista 之後引入了統一的事件提供程序模型和 API，並在 Windows7 得到增強。

由於 ETW 的功能和性能，第三方的日誌系統逐漸式微。ETW 的優點包含
* 提供 Process 與 ETW Session 相分離，應用程序的崩潰不會對 ETW 造成影響
* 能夠動態地啓用和禁用日誌記錄
* 關閉事件跟蹤時間幾乎不消耗系統資源
* 自定義消息格式，便於擴展
* 日誌記錄機制使用每處理器的緩衝區，由非同步 Thread 將這些緩衝區寫入磁盤
* 收集事件的時間戳的時鐘分辨率可精確到 100 ns


### 運作機制
![](https://i.imgur.com/lpxRZPp.png)

ETW 的運作主要有三個角色，Controller、Provider、Consumer。

Controller 負責控制 Session 開關，決定要不要在 Session 接收 Provider。每個 Session 可以同時從多個 Provider 抓取事件，Controller 也能在程式運行的途中開啟或關閉 Session。

Provider 負責提供事件，把事件產生給 Session 處理。Provider 是可以擴展的，能夠自己定義事件欄位。

Consumer 負責透過 Session 接收從 Provider 來的事件。

### 實際用途
#### 籃隊
藍隊的部分，因為有 Callback 機制，可以被動且即時的收到事件。

以 Provider Microsoft-Windows-Threat-Intelligence 為例，它能夠監測 AllocationType、RegionSizes、ProtectionMask，其中的 ProtectionMask 如果是可讀、可寫、可執行的一塊記憶體，則比較有可能是惡意程式。雖然無法斷言只要看到可讀、可寫、可執行的記憶體就代表是惡意程式，不過經過種種方式尋找證據，就可以有一定的信心判斷它是不是惡意程式。

再以 Provider Microsoft-Windows-Kernel-Registry 為例，它可以監測有沒有透過 Registry 實作的 Persistence 行為。比如偵測一個程式是否有寫入以下 Registry
* RunOnce
* Run
* RunServices
* RunServicesOnce
* RunEx
* RunOnceEx

#### 紅隊
既然 ETW 對於籃隊是一個十分有效的工具，那紅隊這邊當然也會有對應策略。

在 Usermode，也就是 Ring3 中，可以透過 Hook EtwEventWrite 的方式攔截事件寫入的函數；或是使用 EtwEventUnregister 取消註冊 ETW Event。之後的文章會介紹一些在 Usermode 繞過 ETW 的方法。

在 Kernelmode，也就是 Ring0 中，有個方法叫做 [GITL(Ghost In The Logs)](https://github.com/bats3c/Ghost-In-The-Logs)，原理是透過 Hook Kernel API NtTraceEvent 達到繞過 ETW 的效果。

## ETW 工具
### 簡介
Windows 有個內建工具 - Logman，可以用來查看 Provider 的資訊和 Session 的狀態，也可以建立 Session 收取事件。

### 使用方法
#### 查看所有 Provider
指令`logman query providers`，回傳結果如下圖。輸出有兩欄，分別是 Provider 和 GUID，其中 GUID 是用來辨別 Provider 的唯一的 ID。
![](https://i.imgur.com/hZ66fcn.png)

#### 查看特定 provider
如果想要知道一個 Provider 更詳細的資訊，只要在剛剛指令的 `providers` 後面加上 Provider 名稱或 GUID。指令 `logman query providers <provider name>` 或 `logman query providers <GUID>`，回傳結果如下。

可以看到除了之前看到的 Provider 和 GUID 外，還多了下面兩個表格。
其中第二個表格是這個 Provider 的 Flag，因為一個 Provider 可能可以監聽不同種類的事件，這些 Flag 就是用來從 Provider 提供的事件中挑選我們需要的事件。假設我們想要監聽其中的 GCKeyword 和 GCHandleKeyword，則輸入的 Flag 就是 `0x1 + 0x10 = 0x11`，等等在建立 Session 時會需要填入 Flag。

第三個表格則是指定的安全層級(Security Level)，每個事件都會有個安全層級。如果選了安全層級高的，則比選擇低的選項也都會一起被啟用。這個安全層級在等等建立 Session 時也需要填入。
```
# logman query providers ".NET Common Language Runtime"

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

#### 使用 session
1. 建立 Session，`logman create trace <session name> -p <provider name / GUID> <flags> <security level> -o <path to log file>`
2. 啟用 Session，`logman start <session name>`
3. 停用 Session，`logman stop <session name>`
4. 刪除 Session，`logman delete <session name>`

以下示範使用 Provider Microsoft-Windows-Kernel-Process 的 Session。在啟用 Session 之後可以稍微打幾個指令，要打會開啟 Process 的指令例如 `whoami`、`net user` 等，之後查看輸出時可以觀察到。
```
:: 1. 建立 Session
logman create trace example -p Microsoft-Windows-Kernel-Process 0x10 0x4 -o C:\example.etl

:: 2. 啟用 Session
logman start example

:: 3. 停用 Session
logman stop example

:: 4. 刪除 Session
logman delete example
```

產生出來的檔案室 etl 檔，可以用 `tracerpt` 轉成 xml 檔，指令如下
`tracerpt example.etl -o example.xml`。

打開檔案後，有在檔案中發現你剛剛打的那些指令的話就成功了。以下是我輸入 `whoami` 指令後產生的輸出。
```
<EventData>
    <Data Name="ProcessID">   23836</Data>
    <Data Name="ProcessSequenceNumber">70760</Data>
    <Data Name="CreateTime">2021-10-05T15:41:44.135049700Z</Data>
    <Data Name="ExitTime">2021-10-05T15:41:44.150097300Z</Data>
    <Data Name="ExitCode">       0</Data>
    <Data Name="TokenElevationType">       1</Data>
    <Data Name="HandleCount">      54</Data>
    <Data Name="CommitCharge">1732608</Data>
    <Data Name="CommitPeak">1937408</Data>
    <Data Name="CPUCycleCount">17963357</Data>
    <Data Name="ReadOperationCount">       0</Data>
    <Data Name="WriteOperationCount">       0</Data>
    <Data Name="ReadTransferKiloBytes">       0</Data>
    <Data Name="WriteTransferKiloBytes">       0</Data>
    <Data Name="HardFaultCount">      15</Data>
    <Data Name="ImageName">whoami.exe</Data>
</EventData>
```

## 其他
今天只是簡單介紹 ETW 和 Windows 內建的工具，但是其實能做到的不只是產生日誌檔案。之後會講解如何寫程式，即時的抓取事件並分析操作。
