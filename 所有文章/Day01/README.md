# 【Day 01】 Zeze 的野望 - 開賽前言

## 前言
根據 [NetMarketShare](https://www.netmarketshare.com/operating-system-market-share.aspx?options=%7B%22filter%22%3A%7B%22%24and%22%3A%5B%7B%22deviceType%22%3A%7B%22%24in%22%3A%5B%22Desktop%2Flaptop%22%5D%7D%7D%5D%7D%2C%22dateLabel%22%3A%22Trend%22%2C%22attributes%22%3A%22share%22%2C%22group%22%3A%22platform%22%2C%22sort%22%3A%7B%22share%22%3A-1%7D%2C%22id%22%3A%22platformsDesktop%22%2C%22dateInterval%22%3A%22Monthly%22%2C%22dateStart%22%3A%222019-11%22%2C%22dateEnd%22%3A%222020-10%22%2C%22segments%22%3A%22-1000%22%7D)，Windows 在全球作業系統市場之中有統治性的地位，佔了八成多的比例。
![](https://i.imgur.com/6rHCyqJ.png)

同時，也因為 Windows 的歷史悠久，更替的版本不計其數，因此常常會考量向下相容，卻也造成了許多「歷史共業」。
以在 Windows10 已經不是預設瀏覽器的 IE(Internet Explorer) 為例。因為它被發現的各種漏洞，經過一修再修和許多更新，導致使用者體驗不佳；新版本要加功能還要考慮到之前的版本，導致新功能開發窒礙難行，所以最後乾脆直接推出 Microsoft Edge。

另外許多版本雖然微軟早已停止支援，然而卻還是有許多人在使用，尤其是工控系統。因為合約的關係，必須繼續使用不再被支援的舊版本，所以有些攻防技巧就算在最新的版本無法使用也不代表它已經不再具備價值。

## 自我介紹
* BambooFox 戰隊隊員
* TSJ 聯隊隊員
* 台灣好厲駭學員
* TeamT5 Intern
* HITCON 工作人員

## 參賽動機
參加了 2020 [台灣好厲駭 - 教育部資訊安全人才培育計畫](https://isip.moe.edu.tw/)的導師制度，在導師的教導下學習到許多有關 Windows 攻防的技巧。

同時也自己研究了各種 Windows 相關的技術，想在今年計畫的最尾端把這一年的學習心得寫成文章，順便記錄自己的學習歷程。

其實這一年也學很多 Ring0 的利用，不過擔心跟隊友撞題，所以這次主題主要圍繞在 Ring3 的部分，對 Windows Kernel 有興趣的朋友可以去看我隊友 [Eric 的文章](https://ithelp.ithome.com.tw/users/20140218/ironman/4166)

## 需要預備什麼
### 先備知識
Windows 攻防方面的先備知識完全可以不用，因為會從基礎講起，只要能正常使用 Windows 就可以看懂了，有寫過 Windows 應用程式會更好。

### 工具
在每篇文章開頭會說需要哪些東西，到時候再裝也可以。

## 文章流程
文章前面會先介紹原理，後面會有實作練習。由於這個系列是希望新手也能看懂的，所以基礎知識的方面都會講解。

部分文章的 POC(Proof Of Concept) 是我自己寫的，雖然沒有到工具的程度，但是也請不要拿去做違法的事，純粹學術用途，如果拿去做惡意使用本人不負責。

程式的部分都有建專案放在我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome)，提供給大家參考。如果文章中有用詞不精確、錯誤的地方，大家可以踴躍討論。

## 大綱
* Microsoft Office Phishing
* Windows 漏洞
* Windows 逆向工程
* DLL Injection
* Shellcode
* Hook
* 後門
* Debug / Anti-Debug / Anti-Anti-Debug
* Shellcode Loader
* Process Hollowing
* 勒索軟體
* Event Tracing for Windows
* 指令混淆
