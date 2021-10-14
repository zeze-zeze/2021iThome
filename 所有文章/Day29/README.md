# 【Day 29】我這不是來了嗎 - 偵測指令混淆

## 環境
* Windows 10 19043
* Python 3.9

## 前情提要
在[【Day 27】Cmd 指令很亂，主辦單位要不要管一下 (上) - Cmd 指令混淆](https://ithelp.ithome.com.tw/articles/10280718)、[【Day 28】Cmd 指令很亂，主辦單位要不要管一下 (下) - Cmd 指令混淆](https://ithelp.ithome.com.tw/articles/10281053) 我們終於把指令混淆的技巧全部認識完畢。

這篇要介紹用來做指令混淆的工具，以及偵測指令是否被混淆的方法。

## Invoke-DOSfuscation
了解了基本的指令混淆原理後，可以來玩玩看專案 [Invoke-DOSfuscation](https://github.com/danielbohannon/Invoke-DOSfuscation)。這是個用 PowerShell 寫的專案，可以用來混淆 Cmd、PowerShell 指令，這個工具當初做出來是為了要讓資安研究員能夠使用這個工具製造出混淆過的 Cmd 指令的測試資料。

### 介面
直接來看看它的使用方法，以下是它十分酷炫的啟動畫面。
```
         ___                 _
        |_ _|_ ____   _____ | | _____
         | || '_ \ \ / / _ \| |/ / _ \____
         | || | | \ V / (_) |   <  __/____|
        |___|_| |_|\_/_\___/|_|\_\___|             _   _
        |  _ \ / _ \/ ___| / _|_   _ ___  ___ __ _| |_(_) ___  _ __
        | | | | | | \___ \| |_| | | / __|/ __/ _` | __| |/ _ \| '_ \
        | |_| | |_| |___) |  _| |_| \__ \ (_| (_| | |_| | (_) | | | |
        |____/ \___/|____/|_|  \__,_|___/\___\__,_|\__|_|\___/|_| |_|

        Tool    :: Invoke-DOSfuscation
        Author  :: Daniel Bohannon (DBO)
        Twitter :: @danielhbohannon
        Blog    :: http://danielbohannon.com
        Github  :: https://github.com/danielbohannon/Invoke-DOSfuscation
        Version :: 1.0
        License :: Apache License, Version 2.0
        Notes   :: if (-not $caffeinated) { exit }


HELP MENU :: Available options shown below:

[*]  Tutorial of how to use this tool             TUTORIAL
[*]  Show this Help Menu                          HELP,GET-HELP,?,-?,/?,MENU
[*]  Show options for payload to obfuscate        SHOW OPTIONS,SHOW,OPTIONS
[*]  Clear screen                                 CLEAR,CLEAR-HOST,CLS
[*]  Execute ObfuscatedCommand locally            EXEC,EXECUTE,TEST,RUN
[*]  Copy ObfuscatedCommand to clipboard          COPY,CLIP,CLIPBOARD
[*]  Write ObfuscatedCommand Out to disk          OUT
[*]  Reset ALL obfuscation for ObfuscatedCommand  RESET
[*]  Undo LAST obfuscation for ObfuscatedCommand  UNDO
[*]  Go Back to previous obfuscation menu         BACK,CD ..
[*]  Quit Invoke-DOSfuscation                     QUIT,EXIT
[*]  return to Home Menu                          HOME,MAIN


Choose one of the below options:

[*] BINARY      Obfuscated binary syntax for cmd.exe & powershell.exe
[*] ENCODING    Environment variable encoding
[*] PAYLOAD     Obfuscated payload via DOSfuscation


Invoke-DOSfuscation>
```

輸入 `Tutorial` 會有基本的使用教學。簡單來說就是可以用 `set command` 選擇要混淆的指令，然後搭配三種混淆選項 `BINARY`、`ENCODING`、`PAYLOAD`。然後混淆的選項是可以疊加的，所以可以用 `undo` 回到上一個混淆狀態，也可以使用 `test` 執行目前的混淆結果。 
```
TUTORIAL :: Here is a quick tutorial showing you how to get your DOSfuscation on:

1) Load a Cmd/PowerShell command (SET COMMAND) or a path/URL to a command.
   SET COMMAND dir C:\Windows\System32\ | findstr calc\.exe
   Or
   SET COMMANDPATH https://bit.ly/L3g1t

2) (Optional) Set FinalBinary (SET FINALBINARY) to be Cmd, PowerShell or None.
   NOTE: If setting a PowerShell command, FinalBinary must be set to PowerShell.
   SET FINALBINARY PowerShell

3) Navigate through the obfuscation menus where the options are in YELLOW.
   GREEN options apply obfuscation.
   Enter BACK/CD .. to go to previous menu and HOME/MAIN to go to home menu.
   E.g. Enter PAYLOAD,CONCAT & then 1 to apply basic concatenation obfuscation.

4) Enter TEST/EXEC to test the obfuscated command locally.
   Enter SHOW to see the currently obfuscated command.

5) Enter COPY/CLIP to copy obfuscated command out to your clipboard.
   Enter OUT to write obfuscated command out to disk.

6) Enter RESET to remove all obfuscation and start over.
   Enter UNDO to undo last obfuscation.
   Enter HELP/? for help menu.

And finally the obligatory "Don't use this for evil, please" :)
```

### BINARY\CMD\1
`BINARY` 混淆選項的兩種可以選，分別是 `CMD` 和 `POWERSHELL`。先看看單獨使用 `BINARY\CMD\1` 混淆結果，每次執行結果不同。以下例子使用了環境變數取出子字串的混淆技巧，從兩個環境變數 `CommonProgramFiles(x86)`、`CommonProgramFiles` 分別取出 `C`、`m`，再補上一個 `d` 構出 `Cmd`。
```
%CommonProgramFiles(x86):~23,1%%CommonProgramFiles:~-9,-8%d
```

### BINARY\CMD\2
`BINARY\CMD\1` 挺簡單的，再來看看單獨使用 `BINARY\CMD\2` 的結果。下面例子使用了 For Loop Value Extraction 混淆技巧，從 `ftype` 出取出 `cmd` 指令。
```
FOR /F "tokens=1 delims=fU" %r IN ('ftype^|findstr mdfi')DO %r
```

### BINARY\CMD\3
最後再看看 `BINARY\CMD\3`。就只是把 `BINARY\CMD\2` 再加上一些垃圾字元而已，包含 `,`、`;`在指令參數之間的空白，然後再放一些 `^` 在指令中。
```
^f^O^r  ;   ,   ;   ,    /^F    ;  ;   ;   ,  ;  ;   ;  "   delims=fLaCK   tokens=    1   "  ,   ;   ;    ,   ,    ;  ;  %8   ,    ,  ;   ;  ^in   ;  ,  ;   ,    ;   (  ;  ,  ,  ,   ,   ,  ,   '   ,   ;    ;  ; Ft^^Y^^p^^E    ;   ;  ,    ^|  ,   ,   ;   ,  ,    F^^iND^^S^^T^^R   ,    ,  ,    ^^m^^df   '  ;   ,    ,  ,   )  ,  ;  ,   ,  ,  ;  DO   ;   ,  ,    %8
```

### BINARY\CMD\3 + PAYLOAD\CONCAT\1
所以只要懂了混淆技巧的原理，混淆指令其實仔細看就能看懂了，是嗎？我們把 `BINARY\CMD\3` 的結果再疊加 `PAYLOAD\CONCAT\1` 看看。一生懸命的看也許還是能勉強理解下面例子使用了環境變數把字串拼接，中間夾雜著一大堆垃圾。
```
cmd.exe /V:ON/C"set Gry=  ,    ,   ;  ,    ^^^^^^^|  ;   ;    ;   , &&set lvz=;  ; &&set Jc9L= ;    ,  &&set GDZ=  ;   &&set B3Vy=   &&set sXnb=^^^^lC&&set 7Bu=o^^^^^^^^C&&set GA=,   ^^^^iN   &&set yqa= ;  , &&set cEVZ=token&&set eZ=^^^^IN^^^^^^^^d^^^^^^^^s^^^^^^^^t^^^^^^^^r&&set hJ=   ,  ;  /^^^^F &&set MO= ;  ;&&set sXH1=  ^^^^^^^^f^^^^&&set Yh= ,  "" &&set Ge=  "" &&set yYMo=;   &&set R5uh=   ,    &&set Zqrd=  &&set mJVE=wt4 &&set 8OU=""&&set Gdf=(  ;&&set ou=  ,  ,  &&set aYH=  &&set 8s=;  ;&&set oe=  &&set 1NU= ,  )  &&set 8ID= ;  ;  ,  %m&&set 9Pa= ,   ;  ,   &&set t6y=;    , &&set Wi=   &&set Vc= ,  ^^^^D^^^^&&set cFM3= &&set CN= ,   &&set CvJ2=  ,   ;   ,  &&set sio=  ,   &&set yU=  ;    ;  &&set shC=,  &&set zyP==l&&set 8i=s&&set 6Y=^^^^^^^^m      '  &&set 1R=;    ;&&set voQ= ,   ,  ^^^^^^^^&&set JLe6=,  ;   ; &&set etCN=  deli&&set RS=,  ,  ;   ;   ,   ^^^^^^^^l^^^^&&set LIYD=    &&set wx8y=  5     &&set 6ov= &&set b3E=  ;  &&set tA=^^^^f^^^^O^^^^R&&set q2MN= &&set ZI=s= &&set 9PRI='    ;&&set 6PiG=m&&set Zyf=  ;  ,&&set oekv= , &&set o5WB=%m &&set qz=A^^^^^^^^s^^^^^^^^s^^^^^^^^&&set hJtC=O  &&call set j0p=%tA%%CvJ2%%1R%%hJ%%yU%%Yh%%oe%%etCN%%6PiG%%8i%%zyP%%mJVE%%B3Vy%%q2MN%%Zqrd%%cEVZ%%ZI%%wx8y%%Ge%%b3E%%ou%%9Pa%%shC%%o5WB%%MO%%cFM3%%sio%%oekv%%Zyf%%Wi%%GA%%8s%%GDZ%%CN%%Gdf%%aYH%%Jc9L%%yYMo%%9PRI%%LIYD%%lvz%%6ov%%t6y%%voQ%%qz%%7Bu%%Gry%%sXH1%%eZ%%R5uh%%RS%%sXnb%%6Y%%JLe6%%1NU%%yqa%%Vc%%hJtC%%8ID%&&cmd.exe /C %j0p:""=!8OU:~1,97!%"
```

### BINARY\CMD\3 + PAYLOAD\CONCAT\1 + PAYLOAD\REVERSE\1
最後再給個魔王題當作這個段落的結尾， `BINARY\CMD\3`+`PAYLOAD\CONCAT\1`+`PAYLOAD\REVERSE\1`
```
cmd /V:ON/C"set C2S=^^^^^^^^d&&set 1aS= ;   ;   &&set fLZ=^^^^&&set 3O= ,  ,&&set FzM=,   ,&&set B9R=  ;   ,&&set 0RIg= ; &&set biOK=""&&set YM6=  ;  ""     tokens=      1   &&set XQS= ;&&set GvAF= &&set Ng=^^^^S^^^^&&set Xs=;   ,&&set czG=   ,  ;&&set dxSn=.aXS=""   ,&&set 39C= &&set 0G=  ; &&set x5= ;  ;   ;   ,&&set 4W30=  ,  &&set 0U7W=^^^^O  ;&&set C0= ;&&set POS5=   ;  ^^^^^^^^a^^^^^^^^S^^^^^^^^S^^^^^^^^oC    ;  ,  ;  ,  ^^^^^^^|  ;  ,&&set hu=/^^^^f    ;  ;  ,   &&set C2a1=   ,  ,  , &&set ho= &&set fK5w= ;   ;   ^^^^D&&set bJ=   &&set H82I=  %w  &&set GTr=  ;   ;   ,   ,  (  &&set ukZx= &&set 1q=ms=&&set 62=;  ^^^^^^^^m^^^^^^^^d^^^^^^^^&&set NT1M=  ;    ^^^^^^^^F^^^^&&set 9G=  ,&&set Zuq=,  &&set RdQM=^^^^IN&&set F97Y=^^^^=^^^^^^^^c    '&&set TJ=  ,  &&set Pl=  &&set aw1= ;   ,   %w&&set 3Qu='  , &&set 94I=  &&set VlL= in&&set c8J=,  ; &&set VP=   deli&&set 1k=^^^^Fo^^^^R&&set ABbl=  , &&set Ahx= ;  ,  &&set tr19= &&set Dn= &&set 6AP=  &&set hI93=  &&set Hf= ,   &&set 5kI= ; &&set Nzt=  ;  ;  )  &&set fz6m=;  ;&&set 0C3= &&set Xu=^^^^r   ,   ; &&set afF5=^^^^t^^^^&&call set HFmo=%1k%%94I%%ABbl%%1aS%%x5%%6AP%%hu%%FzM%%YM6%%ukZx%%VP%%1q%%dxSn%%hI93%%B9R%%bJ%%3O%%9G%%0G%%H82I%%Zuq%%Ahx%%GvAF%%VlL%%4W30%%GTr%%C0%%39C%%5kI%%C2a1%%Hf%%3Qu%%Xs%%POS5%%0C3%%NT1M%%RdQM%%C2S%%fLZ%%Ng%%afF5%%Xu%%ho%%62%%F97Y%%TJ%%XQS%%czG%%Nzt%%c8J%%0RIg%%fK5w%%0U7W%%Pl%%fz6m%%Dn%%tr19%%aw1%&&cmd /C %HFmo:""=!biOK:~0,-1!%"
```

## 偵測混淆
經過 Invoke-DOSfuscation 的洗禮，相信大家都明白不太可能用肉眼看出混淆後的指令。不過如果只是要辨認指令有沒有被混淆的話會不會比較容易呢？在 FireEye 的一篇文章 [Obfuscated Command Line Detection Using Machine Learning](https://www.fireeye.com/blog/threat-research/2018/11/obfuscated-command-line-detection-using-machine-learning.html) 有比較傳統的偵測混淆與機器學習的方法。

### 檢查特徵
使用 Regular Expression 的方式找出被混淆的指令，假設我要找到使用環境變數拼接的混淆技巧，寫出來的規則可能如下。
```
(set [a-zA-Z0-9]+=.*&&)+(call set [a-zA-Z0-9]+=%[a-zA-Z0-9%]+%).*call %.*%
```

如此一來，類似如下的混淆指令就會被偵測。
```
cmd /c "set a=i&&set b=am&&set c=who&&call set d=%c%%b%%a%&&call %d%"
```

然而這種方式卻很容易被繞過，例如我在上述例子的第一個 `&&` 跟 `set` 中間插入一個逗號。這麼做的話上面寫的規則就不適用了。
```
cmd /c "set a=i&&,set b=am&&set c=who&&call set d=%c%%b%%a%&&call %d%"
```

### 設定規則
設定規則的方法比檢查特徵好一些，畢竟規則不會訂得這麼緊。規則除了也可以使用 Regular Expression 之外，也可以透過判斷一些條件決定指令是否被混淆，例如某些字串的數量或指令的總長度。
```
if length() >= 20 and COUNT("&") > 8 and MATCHES_REGEXP(...)
```

不過這個方法比起單純的 Regular Expression 更難維護與測試。

### 機器學習
#### FireEye
機器學習的偵測方法比起前兩種更有彈性，不過能不能用還是要建立在準確度上。根據 FireEye 文章所述，它分別嘗試使用 CNN 做非監督式學習與 Gradient-Boosted Tree-Based Model 做監督式學習。測試資料使用 [Invoke-DOSfuscation](https://github.com/danielbohannon/Invoke-DOSfuscation/tree/master/Samples) 產生的混淆指令，不過沒說正常指令的測試資料怎麼產生的。

實作的細節並沒有透漏太多，只知道 CNN 的部分是直接餵原始指令，並在第一層做 Embedding。Gradient-Boosted Tree-Based Model 則是取出以下幾個特徵，分別是指令長度、`^` 數量、`|` 數量、空白鍵比例、特殊字元比例、字串的 Entropy、字串 `cmd`、`power` 出現的頻率。

結論是 Gradient-Boosted Tree-Based Model 準確度趨近於 1，而 CNN 稍差，這也證實機器學習對於辨識混淆指令是有幫助的。

#### Adobe
另一篇是來自 Adobe 部落格的 [Using Deep Learning to Better Detect Command Obfuscation](https://medium.com/adobetech/using-deep-learning-to-better-detect-command-obfuscation-965b448973e0)，程式碼有開源 [adobe/obfuscation-detection](https://github.com/adobe/obfuscation-detection)。

實作原理也是 CNN，一開始先把指令轉成 One-hot Vector。接著在每一層同時看三個字元，所以第一層每個字元會一次看到三個字元，第二層就一次會看五個字元，以此類推。在最後一層會把上一層的每一列個別加總變成一個 Vector，然後經過 Fully Connected Layer 轉成輸出。輸出是一個二維 Vector，第一項代表預測沒混淆的機率，第二項代表預測混淆的機率，看哪個比較大結果就是什麼。
![](https://i.imgur.com/sCs1diG.png)

這項工具測試結果 F1 Score 為 0.9891，聲稱比 FireEye 的測試結果更好。不過文章中有說在混淆做得不夠多的情況下會有些 False Negative。

使用方法也很簡單，它有 Python Package 直接裝 `pip install obfuscation-detection`。同時支援 Windows 和 Linux 的指令混淆偵測。以下是它的使用範例。
```python=
import obfuscation_detection as od

oc = od.ObfuscationClassifier(od.PlatformType.ALL)
commands = ['cmd.exe /c "echo Invoke-DOSfuscation"',
            'cm%windir:~ -4, -3%.e^Xe,;^,/^C",;,S^Et ^^o^=fus^cat^ion&,;,^se^T ^ ^ ^B^=o^ke-D^OS&&,;,s^Et^^ d^=ec^ho I^nv&&,;,C^Al^l,;,^%^D%^%B%^%o^%"',
            'cat /etc/passwd']
classifications = oc(commands)

# 1 is obfuscated, 0 is non-obfuscated
print(classifications) # [0, 1, 0]
```

## 參考資料
* [Obfuscated Command Line Detection Using Machine Learning](https://www.fireeye.com/blog/threat-research/2018/11/obfuscated-command-line-detection-using-machine-learning.html)
* [Using Deep Learning to Better Detect Command Obfuscation](https://medium.com/adobetech/using-deep-learning-to-better-detect-command-obfuscation-965b448973e0)
* [adobe/obfuscation-detection](https://github.com/adobe/obfuscation-detection)
* [Cmd and Conquer: De-DOSfuscation with flare-qdb](https://www.fireeye.com/blog/threat-research/2018/11/cmd-and-conquer-de-dosfuscation-with-flare-qdb.html)
* [De-DOSfuscator](https://github.com/mandiant/flare-qdb/blob/master/doc/dedosfuscator.md)
