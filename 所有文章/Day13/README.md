# 【Day 13】粗暴後門，Duck 不必 - Windows 簡單後門

## 環境
* Windows 10 1709

## 簡介
### 後門
當入侵者成功取得某台機器的權限後，可能會希望之後仍然能夠持許擁有這個權限。放個後門，下次要進來操作這台機器時就比較容易。

### 簡單後門
這篇要介紹的後門不需要寫任何程式，也不會有艱深的觀念，應該算是一些 Windows 簡單的奇技淫巧。希望讓剛入門的朋友們能有些可以做點壞事的感覺，順便為下一篇比較難的後門做鋪陳。


## Shift 五下
Windows 10 的鎖定畫面如下圖，右下角可以看到一些 Windows 內建功能，原本的功能是用來輔助使用者的。假設入侵者目前可以用遠端桌面訪問受害主機，那就能夠透過竄改掉那些內建功能達到後門的效果。
![Windows 鎖定畫面](https://i.imgur.com/rz0rYPx.jpg)

### 簡介
Windows 內建功能，按五下 shift 會出現 Enable Sticker Key 的視窗，用來啟用 Sticker Key 功能。以下的操作原理就是讓原本使用的 sethc.exe 改成使用 cmd.exe。

### 實作 1 - 改 registry
* 注意事項
    * 改之前可以先備份一下目前的 Registry，以免改完後忘記怎麼改回來
* 步驟
    1. 開啟 Registry Editor，可以在 Windows 搜尋處搜尋 Regedit
    2. 進入到 `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution`
        ![](https://i.imgur.com/QZmcKgL.png)
    3. 按右鍵新增一個機碼，並取名為 sethc.exe
        ![](https://i.imgur.com/He11izp.png)
    4. 最後在 sethc.exe 按右鍵新增一個字串值，名稱為 Debugger，資料為 `C:\Windows\System32\cmd.exe`
        ![](https://i.imgur.com/8heI5EA.png)
    5. 現在連按 5 下 shift 看有沒有一個 cmd 跑出來，有就成功了。可以把螢幕鎖定後再試一次體驗一下後門的感覺。在 cmd 中輸入 `whoami` 還會發現是 System 權限哦！

### 實作 2 - 改 sethc.exe
* 注意事項
    * 這個實作強烈建議在虛擬機上做，以免本機環境髒掉。
    * 需要先修改 `C:\Windows\System32` 的資料夾權限，改成完全控制 (Full Control)。
* 步驟
    1. 打開資料夾 `C:\Windows\System32`
    2. 找到 sethc.exe，把它重新命名為 sethc.exe.bak
    3. 找到 cmd.exe，複製貼上在同個目錄，並重新命名為 sethc.exe
    4. 現在連按 5 下 Shift 就應該會有個 cmd 跑出來

### 其他
除了 Shift 五下之外，同樣的招法也可以使用在鎖定畫面右下角的其他功能中。

![](https://i.imgur.com/gSUJbtk.png)
* 朗讀程式 - Narrator.exe
* 放大鏡 - Magnify.exe
* 螢幕小鍵盤 - osk.exe
* 高對比 - 跟 Shift 五下一樣是 Sethc.exe
* 按下下圖這個 Icon 本身也是一個執行檔 - Utilman.exe
    ![](https://i.imgur.com/knbRCS3.png)

### 偵測
一般的防毒軟體很容易就能發現這類簡單的後門，畢竟一般不會有使用者去換掉 C:\Windows\System32 中的檔案，也不會在這種內建工具的 Regsitry 放 Debugger。

## Run、Runonce
### 簡介
有時一些服務會希望在使用者登入時執行一些指令，因此會用到 Run、Runonce。然而這同時也可以被惡意利用，在使用者登入時執行惡意指令，例如惡意程式在每次使用者登入時檢查程式是否已被執行。Run、Runonce 至今仍是許多 Malware 會使用的技巧。

### 實作
* 注意事項
    * 改之前可以先備份一下目前的 Registry，以免改完後忘記怎麼改回來
* 步驟
    1. 開啟 Registry Editor
    2. 進入其中一個
        * `\HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`
        * `\HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\RunOnce`
        * `\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Run`
        * `\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\RunOnce`
        * `\HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run`
        * `\HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\RunOnce`
    3. 按右鍵新增字串值
    4. 名稱隨意，資料為想要在使用者登入時執行的檔案路徑
        ![](https://i.imgur.com/JgnmV4i.png)
    5. 重新登錄看目標檔案是否執行

### 其他
除了 Run、Runonce 之外，其實還有很多 Registry 可以做到同樣功能。例如 Logon、Startup、RunOnceEx 等等。

### 偵測
* 之後介紹的 Sysinternals，可以使用其中的 autorun.exe 查看
* 之後介紹的 ETW，可以異步接收到目標 registry 的修改狀況
* 可以單純寫程式抓 Run、Runonce 執行的檔案，再判斷它是否為惡意的

## 參考資料
* [大神秘籍：windows中常見後門持久化方法總結](https://kknews.cc/zh-tw/code/442vbzx.html)
* [Windows常见的持久化后门汇总](https://cloud.tencent.com/developer/article/1645257)
