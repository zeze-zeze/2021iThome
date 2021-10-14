# 【Day 05】你逆 - 逆向工程工具介紹

## 環境
* Windows 10 21H1
* IDA PRO 7.5
* Python 3.9
* x64Dbg Aug 2 2020, 13\:56:14

## 前言
雖然這個系列並不是逆向工程為主，但是在探討一些技巧或是講解 POC 原理的時候，難免會需要一些逆向工具的幫忙。

這篇文章會以讓大家能夠看懂其他篇文章為目的撰寫，所以不會講太深入的逆向工程技巧，例如 Deobfuscation、Anti-Anti-Debug 之類的。主要講些基礎的逆向，讓大家能快速的找到目標，並看懂程式在做什麼。

由於這個系列是在講 Windows 的東西，所以介紹的工具、概念都會偏向 Windows，雖然部分知識可以適用於其他作業系統，不過也有些不行。工具的部分是介紹我自己習慣的，但是也會稍微提到一些能做到相同功能的工具。

以下拿大家熟悉的小算盤(calc.exe)開刀。

## 工具
### [IDA](https://hex-rays.com/ida-pro/)
#### IDA 基本功能
這個工具大概已經被講到爛了，幾乎所有講逆向工程的文章都會提到它。雖然很方便，但是要錢。每年都會辦 Plugin 製作比賽，讓程式功能更豐富，GitHub 上還有人整理好 [IDA Plugin](https://github.com/onethawt/idaplugins-list)，然而似乎一陣子沒更新了。

只要動動手指就可以反組譯。
![](https://i.imgur.com/3xBFySW.png)

只要按個鍵盤(F5)就可以反編譯。
![](https://i.imgur.com/9YxuRkE.png)

其他還有可以看 String、Import Function、Export Function 等等的視窗，應該用滑鼠點一點就知道了。

#### IDA Plugin
這裡介紹幾個 [Angelboy](http://blog.angelboy.tw/) 推薦的 Plugin:
* [BinDiff](https://www.zynamics.com/bindiff.html): 可以同時比對兩個程式的相似度，通常是 Patch 前後的兩個版本，或是很可能來自同個家族的 Malware。
* [Signsrch](https://github.com/nihilus/IDA_Signsrch): 能夠搜尋特定的 Signature 自動辨認 Encryption、Compression 等演算法

另外是我覺得非常實用的 Plugin - [LazyIDA](https://github.com/L4ys/LazyIDA)，開發者是台灣人 [Lays](https://github.com/L4ys)。可以做到把選定範圍的位址的值轉成自己要的型別，例如 String、C/C++ Array、Python List，也可以用快捷鍵複製位址或是字串。在逆向時非常省時間。

#### [IDA Python](https://hex-rays.com/products/ida/support/idapython_docs/)
基本上在前面 IDA 基本功能所看到的大多功能，都可以使用 IDA Python 完成。可以自動化 Parse 反組譯後的組語和反編譯後的程式，也可以做 Symbol Rename、變色等等，彈性頗大。

這些在要處理許多重複性工作時十分有幫助，尤其是在分析 Malware 時可能需要反混淆(Deobfuscation)，這時重複性的工作就會很多。或者是如果要分析大型的專案，IDA Python 的自動化腳本也能讓自己快速的有些頭緒。

### [Ghidra](https://github.com/NationalSecurityAgency/ghidra)
一個跟 IDA 並駕齊驅的軟體，最吸引人的點是它是開源的，也就是說免費。基本功能上 IDA 有的它都有，例如反組譯、反編譯等等。
![](https://i.imgur.com/kMLpfAm.png)

Plugin 的部分雖然比 IDA 少一些，但是數量也是頗驚人，GitHub 上也有人整理了 [Ghidra Plugin](https://github.com/AllsafeCyberSecurity/awesome-ghidra)，然而我對 Ghidra 不熟，無法推薦 Plugin。目前的現狀是有些 IDA 有的 Plugin 在 Ghidra 可能沒有，因此需要各位的開源之力，把 IDA Plugin 移植到 Ghidra。

自動化腳本 Ghidra 使用的語言是 Java，用法可以參考[官方網站](https://ghidra.re/ghidra_docs/api/ghidra/app/script/GhidraScript.html)。

### [x64dbg](https://x64dbg.com)
顧名思義就是一個 Debugger。當靜態分析太複雜，導致有些記憶體資料不知道是什麼時，可以直接追進去看。

開始 Debug 一個程式後大致如下圖，如圖所見左上區塊是目前執行的 Instruction，右上是 Register，左下是看 Memory Dump，右下是 Stack。
![](https://i.imgur.com/GbXYWD5.png)

Symbols 這個 Tab 左半邊可以用來觀察載入的 Image，得知 Image Base Address 與完整路徑，右半邊能夠找到對應的 Export Function，鎖定目標下斷點逆向。這功能在這系列的文章會很常用到，因為常常會需要觀察特定的函數。
![](https://i.imgur.com/K5Qs1tI.png)

### [WinDbg](https://www.microsoft.com/en-us/p/windbg-preview/9pgjgd53tn86?activetab=pivot:overviewtab)
這是微軟開發的 Debugger，所以對於 Windows 的相容性也較高。Kernel Debug 也是需要使用 WinDbg。

不過因為我是從 x64Dbg 入手的，操作上還是比較習慣 x64Dbg，所以這個系列大部分在 Debug 時還是會使用 x64Dbg。

### [PE-bear](https://github.com/hasherezade/pe-bear-releases)
一個用來觀察 PE 檔案的軟體，可以知道目前要分析的檔案的架構，例如 Header 和 Section 的資訊。
![](https://i.imgur.com/aEtAAhT.png)

## 實際使用
以下直接拿分析 calc.exe 為例子。

### 用 IDA 觀察
把 calc.exe 匯入 IDA 後，點選 WinMain 後反編譯如下圖。可以看到 Function 基本上都有 Symbol，其實逆向 Windows 內建的東西基本上都會有 Symbol，節省了許多分析時間，所以其實非常快樂。
![](https://i.imgur.com/sFTykns.png)

從程式碼中知道這邊的程式流程
1. 前面三個 Function，[EventRegister](https://docs.microsoft.com/en-us/windows/win32/api/evntprov/nf-evntprov-eventregister)、[EventSetInformation](https://docs.microsoft.com/en-us/windows/win32/api/evntprov/nf-evntprov-eventsetinformation)、[EventWriteTransfer](https://docs.microsoft.com/en-us/windows/win32/api/evntprov/nf-evntprov-eventwritetransfer) 在處理 ETW 的工作，主要目的是記錄事件。至於 ETW 的運作原理，會在後面文章講解。
2. 執行 [ShellExecuteW](https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew)。仔細看其中的參數，並對應到 ShellExecuteW MSDN 連結的說明，主要關注第二根第三個參數。第二個參數是 0，也就是 NULL，代表現在要執行的動作是 Open；第三個參數是 `ms-calculator:` 代表要開啟的物件。根據以上資訊可以發現一個 Windows 冷知識(?)，在資料夾路徑上打 `ms-calculator:` 會跳出小算盤。

### 用 x64Dbg 觀察
假設現在我們想要知道剛剛 IDA 看到的 EventRegister 函數的第一個參數的具體的值，可以用 Debug 的方式追到函數執行之前的地方觀察。

在 x64Dbg 開啟小算盤，載入後首先看到的還不是 WinMain 的地方，所以要做的事是確認目標的位址。點上方 Symbols 的 Tab 可以看目前載入的 Image，其中可以發現 calc.exe 的 Base Address。
![](https://i.imgur.com/Bli7yM6.png)

因為我們用 IDA 打開看的部分就是 calc.exe，所以要看它的 Base Address。得知了 Base Address 後，在 IDA 的 Edit => Segments => Rebase Program，把值改成 x64Dbg 看到的 Base Address，這樣 IDA 跟 x64Dbg 兩邊的位址就一致了。
![](https://i.imgur.com/hyGD8MT.png)

這時再看 IDA 中 WinMain 的位址，並記錄下來。點回去 x64Dbg 上方的 CPU Tab，按下 Ctrl-g 後輸入位址，就可以看到那邊的 Assembly。
![](https://i.imgur.com/zrGN30e.png)

在這個位址按 F2 下斷點，那一行就會變紅色的，等等程式執行到這邊就會停在這。接著按 F9 繼續執行，就會停在下斷點的地方。
![](https://i.imgur.com/K7zDCAX.png)

接下來就可以開始 Debug 這個函數，因為現在目標是 EventRegister 的參數，所以一直按 F8 單步執行到呼叫 EventRegister 那一行。
![](https://i.imgur.com/kJAUDUj.png)

那要怎麼知道參數是什麼呢？這邊要說明一下 [Windows x64 的 Calling Convention](https://docs.microsoft.com/zh-tw/cpp/build/x64-calling-convention?view=msvc-160)，如果是整數參數，從第一個參數開始分別是 RCX、RDX、R8、R9、Stack。也就是說如果要知道第一個參數是什麼，就要看 RCX 的值，以下圖為例就是 `3CB578FC30`。
![](https://i.imgur.com/vxuwWfO.png)

因為這邊的 `3CB578FC30` 是個位址，具體的值放在這個位址中，所以在左下角 Memory Dump 的地方按下 Ctrl-g，並輸入這個位址，就可以看到這個位址中的值。
![](https://i.imgur.com/Z6uzJZJ.png)

到這邊就找到 EventRegister 函數的第一個參數的值了，那這個值其實是 Provider 的 GUID，代表的意義會在後續文章介紹。
