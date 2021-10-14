# 【Day 15】從零開始的 Debug 生活 - Debugger 原理

## 環境
* Windows 10 21H1
* x64dbg Aug 2 2020, 13:56:14
* IDA 7.5

## 前情提要
在 [【Day 05】你逆 - 逆向工程工具介紹](https://ithelp.ithome.com.tw/articles/10268000)有提過逆向工程所需要的工具，其中包含我常用的 Debugger x64dbg。我們也逆過小算盤，認識基本的 Debugger 使用方法，這篇文章也會拿小算盤下手認識 Debugger。

這篇會介紹 Debugger 究竟是怎麼運作的，為什麼可以停在某個斷點，讓使用者可以慢慢的觀察程式的運行流程。由於不同的 Debugger 在處理同個事件上可能會有不同的實作方式，這篇文會以 x64dbg 為例講解 Debugger 的行為。

下面介紹與實際測試的部分都是在 64-bit 的環境，但是 32-bit 其實大同小異。

## Debugger 機制
### Debug Flag
#### 介紹
這是 [PEB](https://www.nirsoft.net/kernel_struct/vista/PEB.html) 結構中的一個成員 `BeingDebugged`，顧名思義它就是用來判斷目前這個 Process 是否正在被 Deubg。開發者在程式中可以利用這個 Flag 判斷是否被 Debug，而做出不同的行為。

Windows API 有相對應的 [IsDebuggerPresent](https://docs.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-isdebuggerpresent) 和 [CheckRemoteDebuggerPresent](https://docs.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-checkremotedebuggerpresent) 函數檢查這個 Flag 的值，回傳為 TRUE 代表程式正在被 Debug。

#### 實際測試
用 x64dbg 可以看到 PEB 的第三個 Byte 是 1，也就是正在 Debug。
![](https://i.imgur.com/VQVQmHu.png)

### Software Breakpoint
#### 介紹
Software Breakpoint（軟體斷點）有三種實作方式，分別是 INT3、Long INT3、UD2，在 x64dbg 可以點選 Options => Preferences => Engine => Default Breakpoint Type 設定。
![](https://i.imgur.com/tmoMwep.png)


x64dbg 預設是使用 `int3`，Opcode 為 `cc`，執行後觸發 EXCEPTION_BREAKPOINT，然後 Debugger 再去處理這個 Exception，達到斷點的效果。

Debugger 會把下斷點的位址的指令改成 `int3`，所以當程式執行到這個位址時，就會觸發上面所提到的 EXCEPTION_BREAKPOINT。執行完 `int3` 後，因為 Instruction Pointer（RIP/EIP）指向的位址是 `int3` 的下一個 Byte，所以 Debugger 在處理 Exception 時會把 Instruction Pointer 減 1，並且把原本要執行的指令填回去。

#### 實際測試
用 IDA 打開 calc.exe，可以看到最後有執行 [ShellExecuteW](https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew)。
![](https://i.imgur.com/ehVJL6x.png)

這邊我們針對 ShellExecuteW 下斷點。因為 Debugger 不會把它做的改變顯示在 Debugger，方便使用者使用，所以我使用了另一個工具觀察。下圖左邊是 x64dbg 在 ShellExecuteW 函數的位址，並且下了斷點；右邊是開源的工具 [HookHunter](https://github.com/mike1k/HookHunter)。
HookHunter 會自動比對檔案與記憶體，看函數有沒有被改動。下圖可以發現，下了斷點後，原本執行的是 `push rbp`，機械碼是 `40 55`。但是因為下斷點的關係，其中的 `40` 被改成 `cc`。而原本的值 `40` 會被 Debugger 存起來等等用來復原。
![](https://i.imgur.com/AAvT1sl.png)

在抵達斷點後可以注意到 HookHunter 已經沒有偵測到檔案和記憶體的不同，因為這時 Debugger 已經把原本被改掉的機械碼 `40` 填回去了。
![](https://i.imgur.com/Nml0Lg6.png)

之後在執行斷點位址的下一個指令後，斷點的位址又被設回去 `int3`，機械碼為 `cc`。
![](https://i.imgur.com/CY5TGpY.png)

### Trap Flag
#### 介紹
Trap Flag（陷阱標誌）是 CPU 中的其中一個 Flag，當 Flag 為 1(set) 時會觸發 EXCEPTION_SINGLE_STEP，其他還有 OF(Overflow Flag)、DF(Direction Flag)、IF(Interrupt Flag)、SF(Sign Flag)、ZF(Zero Flag)、AF(Auxiliary Flag)、PF(Parity Flag)、CF(Carry Flag)。
![](https://i.imgur.com/vvlZK6B.png)

這邊只說明 Trap Flag 的運作原理。在上面介紹 Software Breakpoint 時，最後一個步驟有說明斷點的位址會在執行完後再填回 `int3`，讓斷點繼續發揮作用。因此這邊需要有個機制讓 `int3` 再被填回去，Debugger 就是利用 Trap Flag。

由於 Trap Flag 為 1(set) 時會觸發 EXCEPTION_SINGLE_STEP，Debugger 就可以透過處理這個 Exception 來達到把 `int3` 填回原本程式的效果。在觸發 Exception 之後，Trap Flag 則會被 CPU 設為 0(unset)。

#### 實際測試
一樣在 ShellExecuteW 下斷點並停在這個位址後，可以觀察到右邊視窗有個 TF，代表著 Trap Flag，目前的值為 1(set)。
![](https://i.imgur.com/fTYbGZY.png)

這時會觸發 EXCEPTION_SINGLE_STEP，而 TF 會被重設為 0(reset)。
![](https://i.imgur.com/YSfoQua.png)

### Hardware Breakpoint
#### 介紹
Hardware Breakpoint（硬體斷點） 是透過 Debug Register DR0~DR7 來實作的。其中 DR0~DR3 四個暫存器存放下硬體斷點的位址；DR4、DR5 保留；DR6 存放 Debug Status，用來判斷是踩到哪個斷點；DR7 是 Debug Control，用來設定斷點，其中包含要在對 DR0~DR3 做什麼操作才會觸發斷點，例如讀、寫、執行。

#### 實際測試
使用方法很簡單，就在目標位址按右鍵 => Breakpoint => Hardware，設定斷點。接著就可以在右邊視窗中看到 Debug Register 的改變。
![](https://i.imgur.com/T5xpN4H.png)

## 參考資料
* [调试程序时，设置断点的原理是什么？](https://zhuanlan.zhihu.com/p/34003929)
* [X86 CPU 暫存器 Register 大全](https://finalfrank.pixnet.net/blog/post/22992166)
* [HOW DEBUGGER WORKS](https://mocheng.wordpress.com/2006/05/26/how-debugger-works/)
* [一個偵錯程式的實現（七）斷點](https://www.itread01.com/p/879748.html)
* [wiki: x86 debug register](https://en.wikipedia.org/wiki/X86_debug_register)
* [馬聖豪: 記憶體無痕鉤子 - 硬體斷點 (C++) 實作 Ring3 進程防殺](https://blog.30cm.tw/2019/11/windows-debug-c-ring3.html)
* [Anti-Debug: Assembly instructions](https://anti-debug.checkpoint.com/)
