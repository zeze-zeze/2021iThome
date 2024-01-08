# 【Day 04】CVE 哪有那麼萌 - 找漏洞經驗分享

## 動機
這篇內容講的是我在今年暑假參加 [AIS3 暑期營隊](https://ais3.org/)軟體安全組所做的專題，隊友分別為 [Zero871015](https://github.com/Zero871015)、[hsuck](https://github.com/hsuck)、[r888800009](https://github.com/r888800009/)，主題是「找 CVE 好難」。

AIS3 是一個為期七天的活動，前五天會安排資安業界的講者們授課，剩下兩天是學員們的專題報告。學員要利用營隊這段時間，根據自己所在的組別做出相關研究。一開始我們本來要做 IDA、Ghidra 的擴充功能，不過經過參考資料的搜尋後，發現要在短時間內做出一個沒人做過的擴充功能有些難度。雖然後來發現其實不一定要做出一個完整的作品，而是看作品的想法好不好，例如有的組讀論文並把論文的概念做成擴充功能。

總之當時我們就決定選幾個 CVE 研究，但是看了一些覺得只報告 CVE 好像有點混，於是就想說那來找找看有沒有漏洞。最後總共找到五個漏洞，回報專案負責人並有其中四個目前已經回覆或修復。雖然沒領到 CVE，但仍舊是個有趣的經驗。

附上一張組員合照，今年的 AIS3 因為疫情而在 [Gather](https://www.gather.town/) 舉辦。
![](https://i.imgur.com/0Zc5yS5.png)

## 方法
如前言所介紹，在開始進行專題前會先有相關的授課。聽了來自 [Devcore](https://devco.re/) 的 [Angelboy](http://angelboy.tw/) 和 [Orange](https://orange.tw/) 的分享心得，得到了以下建議。

> 「找漏洞可以從⼩型的專案或是軟體開始，順便建立⾃我信⼼。」 - Angelboy
> 「可以多看看當前的 CVE，說不定沒修好就可以找到另一個 CVE。」 - Orange

根據這些意見，我們決定找目前有的 CVE，並翻找可能有類似漏洞的專案。
從 [MITRE 的 CVE 網站](https://cve.mitre.org/cgi-bin/cvekey.cgi) 搜尋關鍵字，因為我們是軟體安全組，然後我自己是希望找 Windows 相關的漏洞，所以關鍵字是下「windows」。
![](https://i.imgur.com/fHNAMKJ.png)

結果有好幾千個其實也算是意料之內，於是我們從最新的漏洞開始用工人智慧一則一則看，挑選出小型、熟悉的語言、最好有開源的專案。基本上這些條件加上去後，能選擇的就少了許多。當然還是不可能把所有看完，大概只看了 2017 年以後的 CVE。

## CVE-2019-17664
### 漏洞資訊
[CVE-2019-17664](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2019-17664) 是個發生於 Windows 作業系統的 Ghidra 漏洞。它被發現在使用 Ghidra Python 時會執行 cmd.exe，這導致假如有個檔名為 cmd.exe 的惡意程式在同個目錄中，就會在使用 Ghidra Python 時執行這個惡意程式。

### 漏洞成因
看了 [Ghidra Issue \#107](https://github.com/NationalSecurityAgency/ghidra/issues/107) 的討論後發現，其實出問題的不是 Ghidra，而是 Jython。Jython 的 [PySystemState.java](https://github.com/jython/frozen-mirror/blob/master/src/org/python/core/PySystemState.java#L1786) 執行了 cmd.exe，卻沒有檢查可能是執行到目錄下的 cmd.exe 而導致了這個漏洞。

### 官方修復
目前這個 [Jython Issue 2882](https://bugs.jython.org/issue2882) 也有人回報了，預計會在 2.7.3 版本修復。


### 以洞找洞
我們找到幾個有使用 Jython，並且有辦法利用這個 CVE 完成攻擊的專案。這裡有個重點是「能夠利用」，因為如果一個漏洞沒有辦法利用，它可能就不會被視為一個漏洞。

最終我們找到兩個專案，[MeteoInfo](https://github.com/meteoinfo/MeteoInfo/issues/16) 和 [fiji](https://www.google.com/url?q=https://github.com/fiji/fiji&sa=D&source=editors&ust=1631464327075000&usg=AOvVaw3NVHb0X9FyBNmY3K4SIPqs) 能夠利用這個漏洞。

### 漏洞回報
漏洞已回報在 [MeteoInfo Issue #16](https://github.com/meteoinfo/MeteoInfo/issues/16) 和 [fiji Issue #291](https://github.com/fiji/fiji/issues/291)。目前只有 MeteoInfo 專案負責人回覆了「Thanks for this issue report!」。


## CVE-2020-35112
### 漏洞資訊
[CVE-2020-35112](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2020-35112) 是個發生於 Windows 作業系統的 FireFox 漏洞。當一個使用者下載了一個檔案，假設檔名為 test，且沒有副檔名，則當使用者點選下面的下載面板想要開啟它時，如果同個目錄下有檔名為 test.exe 或 test.bat 的檔案，它就會被執行。

### 漏洞成因
程式中使用了 ShellExecuteExW 執行指令，在打開沒有副檔名的 test 檔案時會執行 test.exe 或 test.bat，這也算是 Windows 的歷史共業。

### 官方修復
後來的修復方法是在 Windows 中假設現在下載的是一個沒有副檔名的檔案，則不會嘗試開啟它，而是開啟檔案所在的目錄。

### 以洞找洞
我們找到 [FarManager](https://github.com/FarGroup/FarManager) 這個專案也有相同問題，它是一個 Command Line 程式，用來瀏覽或管理目錄與檔案。我們發現它也有相同的問題，假設同個目錄有 test 檔案與 test.exe 或 test.bat 檔案，當 FarManager 試圖開啟 test 檔案時，會執行到 test.exe 或 test.bat。

### 漏洞回報
漏洞回報在 [FarManager Issue #428](https://github.com/FarGroup/FarManager/issues/428)，目前已經修復了。


## CVE-2021-21237
### 漏洞資訊
[CVE-2021-21237](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2021-21237) 是個發生於 Windows 作業系統的 [git-lfs](https://github.com/git-lfs/git-lfs) 漏洞。如果在執行 git-lfs 指令的同個目錄下有檔名為 git.exe 或 git.bat 的惡意程式，它們會被執行。

### 漏洞成因
專案使用了 golang 1.8 的 exec.Command 來執行 git。它預設會先找目前所在目錄的執行檔，而不是環境變數中的，導致如果有個 repo 中放有檔名為 git.bat 或 git.exe 的惡意程式，git-lfs 會執行到它們而不是使用者環境中的 git。

### 官方修復
在專案中的 exec.Command 外面包一層自己定義的 subprocess.ExecCommand，其中會對指令做檢查。

 golang 那邊後來也把 exec.Command 的預設行為修掉了，所以就算是舊版本的 git-lfs 也不會觸發漏洞。

### 以洞找洞
我們在重現這個漏洞時發現一個狀況，就是如果沒有下載 git，也就是環境變數中沒有 git 指令，在這個情況下假設同個目錄中有 git.bat 或 git.exe，它們就會被執行。

後來經過跟 git-lfs 的負責人溝通後，發現原因是 Windows 的環境變數中包含了當前目錄，這會發生在環境變數 Path 中是以分號 ; 結尾的情況。

在 cmd 中打 `set | find "Path"` 指令會出現許多路徑。
在這個例子中環境變數是沒有包含當前目錄的:
```
Path=C:\Windows;C:\Windows\System32
```

下面這個例子就會包含當前目錄:
```
Path=C:\Windows;C:\Windows\System32;
```

### 漏洞回報
因為這個專案有提供 [Security Policy](https://github.com/git-lfs/git-lfs/security/policy) 說明如何回報漏洞，所以一開始是用 Email 與負責人聯繫。後來我發了 [PR #4562](https://github.com/git-lfs/git-lfs/pull/4562)，負責人也給予了正式的回應。但是這個專案的 Workflow 太難通過了，還要寫複雜的測試程式，所以最後還是交給他們在 [PR #4603](https://github.com/git-lfs/git-lfs/pull/4603) 修復。

一開始 git-lfs 的負責人不認為這是他們的問題，因為他們覺得環境變數包含當前目錄是一件不該發生的事。後來我們跟負責人解釋說有些 Windows 的版本預設情況下的環境變數就會包含當前目錄，例如 Windows Server 2019，但是他們還是覺得這算是 Windows 的責任。然而基於安全考量，他們還是願意在程式中檢查這個問題。

## 心得
這次活動找到的漏洞難度很低，性質也很相近，都是 Windows 處理檔案的問題造成的，只是分別使用 C、Java、GO 語言。雖然難度不高，但是能在短短的營隊期間就達成這樣的成果，我認為還算是滿意。而且經過這次經驗，我覺得我和找漏洞之間的距離又更近了一步，期許自己以後能夠發一個超猛 CVE。
