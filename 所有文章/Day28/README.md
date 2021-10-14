# 【Day 28】Cmd 指令很亂，主辦單位要不要管一下 (下) - Cmd 指令混淆

## 環境
* Windows 10 19043
* System Monitor v13.01

## 前情提要
在[【Day 27】Cmd 指令很亂，主辦單位要不要管一下 (上) - Cmd 指令混淆](https://ithelp.ithome.com.tw/articles/10280718)我們說明了指令混淆的用處與偵測方法，也介紹一些指令混淆的技巧，這篇要繼續介紹更多混淆的技巧。

## Cmd 混淆技巧
### 括弧(Parentheses)
括弧中的指令會被當作是一組指令，無意義的括弧可以用來混淆指令，以下舉個例子。
```
# cmd /c (  (powershell))
Windows PowerShell
Copyright (C) Microsoft Corporation. All rights reserved.
```

這個混淆技巧可以繞過動態偵測，如下圖 Sysmon 日誌。
![](https://i.imgur.com/A07Pn54.png)

### 逗號與分號
`,`、`;` 在指令中可以取代空白作為指令參數之間的分割字元，它們可以任意被插入在參數之間需要空白的地方，以下用 `netstat` 舉例。
```
# cmd;;,,/c,;netstat

Active Connections
```

這個混淆技巧可以繞過動態偵測，如下圖 Sysmon 日誌。
![](https://i.imgur.com/aUGAvJR.png)

### Call
`call` 在 Cmd 指令中用來執行另一段指令，雖然它本身不是用來做混淆的，但是卻可以讓混淆的行為更低調。首先看看下面的例子，可以觀察到例子中用 `&&` 串接兩個指令，第一個設定環境變數，第二個印出環境變數。然而在沒有 `call` 或 `cmd /c` 的情況下，直接 `echo` 不會把環境變數轉成設定的值，因為同個 Cmd.exe Session 不會自動轉換環境變數。
```
# cmd /c "set test=net&&echo %test%"
%test%

# cmd /c "set test=net&&call echo %test%"
net

# cmd /c "set test=net&&cmd /c echo %test%"
net
```

`call` 和 `cmd /c` 都會轉換環境變數，但是兩者的差別在於 `call` 不會產生 Child Process；而 `cmd /c` 會。因此使用 `call` 會比 `cmd /c` 更低調。

### For-Loop Encoding
與 For Loop Value Extraction 不同，For-Loop Encoding 不會使用 Tokens、Delims 的方式拼湊出指令，而是直接寫 For 迴圈把需要的字元從環境變數的目標 Index 取出。

首先先來看看指令中 For 的用法，以下的例子會把 `(0 1 2 3)` 照順序印出來。
```
# cmd /c "for %a in (0 1 2 3) do echo %a"

# echo 0
0

# echo 1
1

# echo 2
2

# echo 3
3
```

也可以在 For 迴圈後加入條件判斷，以下例子判斷如果目前迴圈跑到的值為 `a` 就印出 `haha`。
```
# cmd /c "for %a in (0 1 2 a) do if %a==a (call echo haha) else (call echo %a)"

# if 0 == a (call echo haha )  else (call echo 0 )
0

# if 1 == a (call echo haha )  else (call echo 1 )
1

# if 2 == a (call echo haha )  else (call echo 2 )
2

# if 3 == a (call echo haha )  else (call echo 3 )
3

# if a == a (call echo haha )  else (call echo a )
haha
```

以上的 For 迴圈使用方法再配上前面環境變數的混淆技巧，就可以做到把指令混淆的效果。以下例子最終會執行 `netstat /ano`。先設兩個環境變數 `final`、`unique`，`final` 會存放最終要執行的指令，一開始是空的。接著迴圈的每一輪都會從 `unique` 取出一個字元並添加在 `final` 後面。最後會判斷目前迴圈跑到的 Index 是否為 `a`，是的話就 `call %final%` 執行指令。
```
# cmd /c "set final= &&set unique=nets /ao&&for %a in (0 1 2 3 2 6 2 4 5 6 0 7 a) do if %a==a (call %final%) else (call set final=%final%%unique:~%a,1%)

# if 0 == a (call %final% )  else (call set final=%final%%unique:~0,1% )

# if 1 == a (call %final% )  else (call set final=%final%%unique:~1,1% )

# if 2 == a (call %final% )  else (call set final=%final%%unique:~2,1% )

# if 3 == a (call %final% )  else (call set final=%final%%unique:~3,1% )

# if 2 == a (call %final% )  else (call set final=%final%%unique:~2,1% )

# if 6 == a (call %final% )  else (call set final=%final%%unique:~6,1% )

# if 2 == a (call %final% )  else (call set final=%final%%unique:~2,1% )

# if 4 == a (call %final% )  else (call set final=%final%%unique:~4,1% )

# if 5 == a (call %final% )  else (call set final=%final%%unique:~5,1% )

# if 6 == a (call %final% )  else (call set final=%final%%unique:~6,1% )

# if 0 == a (call %final% )  else (call set final=%final%%unique:~0,1% )

# if 7 == a (call %final% )  else (call set final=%final%%unique:~7,1% )

# if a == a (call %final% )  else (call set final=%final%%unique:~a,1% )

Active Connections
```

這個混淆技巧可以繞過動態偵測，如下圖 Sysmon 日誌。
![](https://i.imgur.com/8FBaK41.png)

### Reversal
這個混淆技巧也是使用 For 迴圈，不過參數中會使用 `/L`，用法有點像 Python 的 `in range()`，使用者可以自己定義迴圈的範圍。以下例子使用 For 迴圈從 3 開始，每次減 1，直到 0 為止，每次迴圈都會印出目前的數字。
```
# cmd /c "for /L %a in (3 -1 0) do echo %a"

# echo 3
3

# echo 2
2

# echo 1
1

# echo 0
0
```

知道 `for /L` 的使用方法之後，直接看看它用來混淆的例子。我們設一個環境變數 `rev` 存 `netstat /ano` 的反轉字串，然後用 For 迴圈從最後一個字元開始往回一一添加在環境變數 `final` 後。當迴圈跑到 0 時就執行 `%final%`。 
```
# cmd /c "set final= &&set rev=ona/ tatsten&&for /l %a in (11 -1 0) do call set final=%final%%rev:~%a,1%&&if %a==0 call %final%

# call set final=%final%%rev:~11,1%  && if 11 == 0 call %final%

# call set final=%final%%rev:~10,1%  && if 10 == 0 call %final%

# call set final=%final%%rev:~9,1%  && if 9 == 0 call %final%

# call set final=%final%%rev:~8,1%  && if 8 == 0 call %final%

# call set final=%final%%rev:~7,1%  && if 7 == 0 call %final%

# call set final=%final%%rev:~6,1%  && if 6 == 0 call %final%

# call set final=%final%%rev:~5,1%  && if 5 == 0 call %final%

# call set final=%final%%rev:~4,1%  && if 4 == 0 call %final%

# call set final=%final%%rev:~3,1%  && if 3 == 0 call %final%

# call set final=%final%%rev:~2,1%  && if 2 == 0 call %final%

# call set final=%final%%rev:~1,1%  && if 1 == 0 call %final%

# call set final=%final%%rev:~0,1%  && if 0 == 0 call %final%

Active Connections
```

上面這個例子已經讓人眼花撩亂了，不過還可以更混淆一點。`for /L` 後面接的範圍每次不一定只能遞減 1，可以遞減任意值。所以原本的環境變數 `rev` 是放 `netstat /ano` 的反轉字串，現在可以在指令之間塞入混淆用的字元。以下例子把原本的 `rev` 字串的每個字元之間插入 `X`，並把 `for /L` 的遞減值改為 2。
```
# cmd /c "set final= &&set rev=oXnXaX/X XtXaXtXsXtXeXn&&for /l %a in (22 -2 0) do call set final=%final%%rev:~%a,1%&&if %a==0 call %final%

# call set final=%final%%rev:~22,1%  && if 22 == 0 call %final%

# call set final=%final%%rev:~20,1%  && if 20 == 0 call %final%

# call set final=%final%%rev:~18,1%  && if 18 == 0 call %final%

# call set final=%final%%rev:~16,1%  && if 16 == 0 call %final%

# call set final=%final%%rev:~14,1%  && if 14 == 0 call %final%

# call set final=%final%%rev:~12,1%  && if 12 == 0 call %final%

# call set final=%final%%rev:~10,1%  && if 10 == 0 call %final%

# call set final=%final%%rev:~8,1%  && if 8 == 0 call %final%

# call set final=%final%%rev:~6,1%  && if 6 == 0 call %final%

# call set final=%final%%rev:~4,1%  && if 4 == 0 call %final%

# call set final=%final%%rev:~2,1%  && if 2 == 0 call %final%

# call set final=%final%%rev:~0,1%  && if 0 == 0 call %final%

Active Connections
```

這個混淆技巧可以繞過動態偵測，如下圖 Sysmon 日誌。
![](https://i.imgur.com/7bHmYyf.png)

### FinCoding
全名為 Fin-Style Encoding，因為 APT 組織 FIN7 的使用而得名。原理其實就是把指令的其中幾個字元替換掉，之後再改回來而已。先看看基本的字串取代方法，下面例子把 `zXzX` 的 `X` 改成 `e`，變成 `zeze`。
```
# cmd /c "set a=zXzX&&call echo %a:X=e%
zeze
```

混淆指令也差不多，以下例子把原本的 `netstat /ano` 一開始先設成 `neXsXaX /ano`，之後再把 `X` 改成 `t`。
```
# cmd /c "set cmd1=neXsXaX /ano&&call set cmd2=%cmd1:X=t%&&call %cmd2%"

Active Connections
```

這個混淆技巧可以繞過動態偵測，如下圖 Sysmon 日誌。
![](https://i.imgur.com/633Kf4J.png)



## 參考資料
* [danielbohannon/Invoke-DOSfuscation](https://github.com/danielbohannon/Invoke-DOSfuscation)
* [dosfuscation-report.pdf](https://www.fireeye.com/content/dam/fireeye-www/blog/pdfs/dosfuscation-report.pdf)
* [Commandline Obfusaction](https://www.ired.team/offensive-security/defense-evasion/commandline-obfusaction#batch-for-delims-tokens)
* [An A-Z Index of Windows CMD commands](https://ss64.com/nt/)
* [APT的思考: CMD命令混淆高级对抗](https://cloud.tencent.com/developer/article/1633973)
