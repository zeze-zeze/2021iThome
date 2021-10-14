# 【Day 27】Cmd 指令很亂，主辦單位要不要管一下 (上) - Cmd 指令混淆

## 環境
* Windows 10 21H1
* System Monitor v13.01

## 前情提要
在[【Day 23】為美好的 Windows 獻上 ETW - Event Tracing for Windows](https://ithelp.ithome.com.tw/articles/10279093) 介紹了 Windows 的 ETW(Event Tracing for Windows)，其中雖然沒有實作，但是實際上有可以取得 Cmd 指令的 Provider。另外在[【Day 26】我們與 1102 的距離 - Bypass Clear Log Event](https://ithelp.ithome.com.tw/articles/10280313) 介紹了 Event Log，使用日誌也可以觀察使用者輸入的 Cmd 指令。

上述兩篇講的都是藍隊的應用，因此這一篇輪到紅隊反擊。紅隊除了直接針對 Windows 的功能繞過之外，還可以透過混淆指令或程式的方式增加藍隊的防守難度。

## 混淆
對於紅隊來說，會希望藍隊分析的成本提高，讓自己的指令或程式比較不容易被理解或是偵測。假設藍隊要辨別現在要執行的指令是不是惡意的，可能會透過指令的一些特徵確認，那紅隊就可以利用混淆的方式繞過那些特徵。

紅隊在成功入侵一台機器後，會盡量使自己做的事情不容易被藍隊發現。所以攻擊者可以把指令混淆，讓資安研究員無法輕易知道攻擊者目前做了哪些壞事。如此一來，攻擊者的入侵程度可能就會被錯估，導致損失持續擴大。

APT 組織 [FIN8](https://attack.mitre.org/groups/G0061/) 曾利用混淆的技巧在釣魚文件上，透過混淆過的 Macro 來隱藏執行的指令。釣魚文件的介紹可以參考之前的文章[【Day 02】Word 很大，你要看一下 - Microsoft Office Phishing](https://ithelp.ithome.com.tw/articles/10265704)。

混淆的目標可以是 Cmd 指令、Powershell 指令、Python、Javascript、C#、C/C++ 程式等等，這篇主要會說明 Cmd 指令的混淆原理與偵測方法。

## 偵測方式
首先要先介紹兩種主要的偵測方法，分別是靜態偵測與動態偵測，了解之後會更好理解混淆技巧的使用情境。

### 靜態偵測
靜態的偵測方式一般是指從檔案、Registry 取得目標，可以直接讀取內容做判斷。

靜態偵測的優點如下：
* 效率較高
* 只要有目標就可以偵測

缺點如下：
* 比較容易繞過

例如現在看到一個 evil.cmd 檔案，檔案內容如下，裡面有加入一個簡單的插入符號混淆，在文章後面會介紹。靜態偵測就是會拿到原本的檔案內容。
```
::evil.cmd
cmd /c who^ami
```

### 動態偵測
相對於靜態偵測，動態偵測可以避免掉部分的混淆，因為有些混淆會在執行前載入記憶體時被解析。可以使用 ETW 或 Event Log 取得指令。

* 優點
    * 比較不容易繞過
* 缺點
    * 效率較低
    * 需要滿足特定的需求，例如執行

繼續上述靜態偵測的例子，雖然執行的指令是 `who^ami`，但是從 [Sysmon](https://docs.microsoft.com/en-us/sysinternals/downloads/sysmon) 產生的 Log 來看卻是沒被混淆的指令，如下圖。
![](https://i.imgur.com/pRaaATW.png)


## Cmd 混淆技巧
在 FireEye 的 [Dosfuscation 白皮書](https://www.fireeye.com/content/dam/fireeye-www/blog/pdfs/dosfuscation-report.pdf)有講解可能被利用的混淆方法，其中說明有哪些 APT 曾經使用了哪些技巧，在文末也有介紹可能的偵測方式。

### 環境變數(Environment Variable)
在 Cmd 中輸入 `set` 會輸出環境變數，輸出大致如下，這裡截取片段。
```
# set
ALLUSERSPROFILE=C:\ProgramData
ANSICON=212x32766 (212x49)
ANSICON_DEF=7
```

在 Cmd 指令中，我們可以透過兩個 `%` 包住環境變數名稱來代表它的值，以 `ALLUSERSPROFILE` 為例。
```
# echo %ALLUSERSPROFILE%
C:\ProgramData
```

我們可以利用環境變數的子字串湊出我們想要執行的指令，舉例如下。數字 2 代表從第幾個字元開始，數字 3 則代表子字串長度。
```
# echo %ALLUSERSPROFILE:~2,3%
\Pr
```

所以假設我們要執行 `whoami` 指令，我們可以透過把多個子字串拼接在一起來達成，舉例如下。`whoami` 的 `W`、`O`、`i` 從環境變數 SystemRoot 拿；`a`、`m` 從環境變數 Tmp 拿；`h` 則直接使用字串。把所有字元拼在一起就可以執行 `whoami` 了。
```
# echo %SystemRoot%
C:\WINDOWS

# echo %Tmp%
C:\Users\user1\AppData\Local\Temp

# cmd /c "%SystemRoot:~3,1%h%SystemRoot:~7,1%%tmp:~-7,1%%tmp:~-2,1%%SystemRoot:~4,1%"
laptop-nqh6ma4o\zezec
```

不過這個方法只能繞過靜態偵測，使用 Sysmon 觀察還是會是原本的指令。
![](https://i.imgur.com/cj0a20Z.png)



### For Loop Value Extraction
這個混淆技巧主要是利用 Windows 內建指令的輸出取得目標字串。與環境變數不同的是它可以使用在任何指令上，而不僅限於 `set`。`For` 可以搭配 Delims、Tokens 使用，以下舉個例子。其中 Delims 是分割字元，下面例子是 `c`，所以會把 `echo abcde` 的輸出 `abcde` 用 `c` 切割；Tokens 則是 Index，從一開始數，下面例子是 `2`，所以會拿到 `abcde` 中的 `de`。
```
# for /f "Delims=c Tokens=2" %a in ('echo abcde') do echo %a
# echo de
de
```

以這個邏輯來看，我們可以把要輸入的指令轉成這種格式。使用 Windows 內建指令可以避免產生多餘的 Child Process，除了可以用 `set` 之外，還有 `ftype`、`assoc`、`ver` 等等。以下舉 `PowerShell` 為例，從 `set` 中先用 Findstr 找到目標的 `PowerShell` 字串，再用 Tokens 和 Delims 截取。
```
# set | findstr PSM
PSModulePath=C:\Program Files\WindowsPowerShell\Modules;C:\WINDOWS\system32\WindowsPowerShell\v1.0\Modules;C:\Program Files (x86)\Microsoft SQL Server\150\Tools\PowerShell\Modules\


# cmd /c "for /f "Delims=s\ Tokens=4" %a in ('set^|findstr PSM') do %a"
# PowerShell
Windows PowerShell
Copyright (C) Microsoft Corporation. All rights reserved.
```

這個技巧可以用來繞過靜態與動態偵測，下圖為 Sysmon 日誌。
![](https://i.imgur.com/AGh4SgU.png)

### 插入符號(Caret)
插入符號，又稱脫字符號，指的是 `^`，它有以下的特性。
```
:: a 會被執行
^a => a

:: 第 2 個 ^ 和 a 被執行
^^^a => ^a

:: 第 2、4 個 ^ 和 a 被執行
^^^^^a => ^^a
```

根據這個特性我們可以構造出指令，中間夾帶著插入符號。舉執行 `powershell` 為例。
```
# cmd /c p^owe^rsh^ell
Windows PowerShell
Copyright (C) Microsoft Corporation. All rights reserved.
```

不過這個方法只能繞過靜態偵測，下圖為 Sysmon 日誌。
![](https://i.imgur.com/4YD2HaM.png)

### 雙引號(Double Quotes)
雙引號在指令中被當作是一個連接用的字元，所以在指令中可以正常的使用它，將雙引號插入在任何位置。
```
# cmd /c p"owe"""rshe""ll
Windows PowerShell
Copyright (C) Microsoft Corporation. All rights reserved.
```

雙引號比插入符號更常被一般的程式使用；而且不會有像是插入符號那樣連續兩個 `^` 被當作一個 `^` 的情形，所以可以存在在更深層的 Child Process；雙引號的連接特性也適用於許多除了 Cmd 之外的程式，例如 PowerShell。最棒的是它可以躲過動態偵測，如下圖 Sysmon 日誌。基於以上原因，比起插入符號，雙引號更常被用來做混淆。
![](https://i.imgur.com/Iu9xKd4.png)



## 參考資料
* [danielbohannon/Invoke-DOSfuscation](https://github.com/danielbohannon/Invoke-DOSfuscation)
* [dosfuscation-report.pdf](https://www.fireeye.com/content/dam/fireeye-www/blog/pdfs/dosfuscation-report.pdf)
* [Commandline Obfusaction](https://www.ired.team/offensive-security/defense-evasion/commandline-obfusaction#batch-for-delims-tokens)
* [An A-Z Index of Windows CMD commands](https://ss64.com/nt/)
* [APT的思考: CMD命令混淆高级对抗](https://cloud.tencent.com/developer/article/1633973)
