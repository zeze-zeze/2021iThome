# 【Day 02】Word 很大，你要看一下 - Microsoft Office Phishing

## 環境
* Microsoft Office 2019

這篇文主要介紹我在 Microsoft Office 2019 嘗試並且測試成功的手法為主，因為微軟在對付釣魚攻擊這方面也是有很多更新，所以有些在舊版本可行的攻擊手法在新版本可能會變得無法使用。

## 釣魚(Phishing)
一般聽到釣魚，可能第一個聯想到的是釣魚網站，透過引導使用者到需要偽造的網站竊取帳號密碼。Office 的釣魚其實也是差不多的概念，誘騙使用者給予權限或允許一些功能導致惡意指令的執行。

一般人聽到駭客入侵，也許會覺得防毒軟體不給力；有資安概念的人會知道可能是什麼 0-day, 1-day, ... 等等漏洞。但是其實社交工程也是一個很大的威脅，從基層人員下手，取得低權限的帳戶後再進行提權。

比較常見到的案例是有人想要下載盜版軟體，然後從不知名網站下載壓縮檔，結果執行後就中毒了。還有電子郵件收到檔案，沒有確認過來源就直接下載執行。

種種原因，目前資安法也要求人員的資安訓練並要達到一定時數。

## Office 釣魚手法
### 巨集(Macro)
#### 介紹
根據微軟的介紹，巨集是可以用來將重複性作業自動化的一系列命令，且可以在需要進行該作業時執行。雖然巨集很方便，但是也被惡意程式濫用。

#### VBA Macro
建立一個 word(.docx) 檔案，按下 Alt+F11 會進入 Macro Editor。
![](https://i.imgur.com/GZ2fV4J.png)

在程式碼中輸入以下程式。其中 [Document_Open](https://docs.microsoft.com/en-us/office/vba/api/word.document.open) 是一個事件，會在檔案開啟時觸發。[Shell](https://docs.microsoft.com/zh-tw/office/vba/language/reference/user-interface-help/shell-function) 則是執行程式，參數要放路徑。
```
Private Sub Document_Open()
  a = Shell("C:\Windows\System32\calc.exe")
End Sub
```

存檔要存成 docm 檔，這才支援 Macro 語法。
![](https://i.imgur.com/f9Hw3Ua.png)

存檔完打開後會出現已經停用巨集，因為預設不是啟用的。所以如果有個麻瓜不知道這按下去可能會執行某個程式就開啟小算盤了。
![](https://i.imgur.com/mOzi4bz.png)

#### Excel 4.0 Macro
這是比較舊的巨集，在 1992 年 Excel 4.0 出現的功能，不過現在還是可以使用，它跟 Excel 5.0 的 VBA Macro 差了不少，這邊也介紹它的玩法。

打開 Excel(.xlsx)，在左下角工作表按右鍵 => 插入。
![](https://i.imgur.com/Wx3cQWI.png)

選擇 Excel 4.0 巨集表。
![](https://i.imgur.com/CYS5CPi.png)

在座標欄位輸入 Auto_Open，代表開啟後自動執行。EXEC 和 HALT 則分別代表執行和結束 Macro。
![](https://i.imgur.com/EjZIh2L.png)

最後存成 xlsm 檔案，否則不能使用巨集。
![](https://i.imgur.com/4IfyPK0.png)

### 超連結
超連結功能是連結到外部資源，但是也可以被惡意用來連結到釣魚網站或是本機的檔案。

按下插入 => 連結，網址的部分可以選擇本機檔案或是釣魚網站的連結，顯示的文字也可以誤導使用者。
![](https://i.imgur.com/2oNc4PM.png)

如果沒仔細看就會就不小心按了確認就執行檔案或者是不知道自己連到釣魚網站了。
![](https://i.imgur.com/ZMuRlOy.png)

## 防範方法
這裡直接引用[趨勢科技](https://www.trendmicro.com/zh_tw/what-is/phishing.html#how-to-prevent-phishing-tm-anchor)寫的部分內容。
![](https://i.imgur.com/fjizQCm.png)

## 其他
這篇有提到的範例會放在我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Word%E5%BE%88%E5%A4%A7%E4%BD%A0%E8%A6%81%E7%9C%8B%E4%B8%80%E4%B8%8B)。

還有很多玩法可以參考以下參考資料，這篇主要還是讓大家認識基本的釣魚攻擊可能的行為原理，藉此產生更多危機意識。

## 參考資料
* [Phishing with MS Office](https://www.ired.team/offensive-security/initial-access/phishing-with-ms-office)
* [Embed or link to a file in Word](https://support.microsoft.com/en-us/office/embed-or-link-to-a-file-in-word-8d1a0ffd-956d-4368-887c-b374237b8d3a)
* [oletools](https://github.com/decalage2/oletools)
* [Macro: MSDN](https://support.microsoft.com/zh-tw/office/%E5%95%9F%E7%94%A8%E6%88%96%E5%81%9C%E7%94%A8-office-%E6%AA%94%E6%A1%88%E4%B8%AD%E7%9A%84%E5%B7%A8%E9%9B%86-12b036fd-d140-4e74-b45e-16fed1a7e5c6)
* [Old school: evil Excel 4.0 macros (XLM)](https://outflank.nl/blog/2018/10/06/old-school-evil-excel-4-0-macros-xlm/)
* [How to Reverse Office Droppers: Personal Notes](https://marcoramilli.com/2020/08/24/how-to-reverse-office-droppers-personal-notes/)
