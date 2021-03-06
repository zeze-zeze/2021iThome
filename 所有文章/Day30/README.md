# 【Day 30】再…再一年 - 完賽心得與瀏覽數分析

## 完賽心得
### 起
三十天的 iThome 鐵人賽終於完賽，如同我們的隊名，**「肝已經，死了」**。參賽之前覺得`每天`發一篇`還好吧`，只是把之前的學習筆記拿來`改一改`、`寫一寫`，應該`不需要花太久時間`。

### 承
直到`比賽期間`才發現，`一天`寫一篇`太難了`，要在兼顧`文章品質`與`內容多寡`的情況下，一天一篇`對我而言`實在`不可能`，況且平常也有其他的事，不過`好險`我有提早開始`囤文章`。

在參加這次鐵人賽前，身為 iT 邦幫忙`資深`的`潛水艇`，我從來都`沒有`按過文章的`讚`，更別說`留言回覆`，事實上我是在開賽`前幾週`才`創帳號`。

### 轉
於是我在這次比賽被狠狠的`制裁`了。直到這次鐵人賽`途中`我才`發現`，不論是`讚`、`留言`，甚至是`瀏覽數`，都對`發文者`來說是一個`很大的鼓勵`。

另外參賽的這段期間，我很好奇究竟`為什麼`我的`瀏覽數`這麼`低`，明明文章的`難度`、`價值`應該`不會`比一些瀏覽數高的`差`才對。

`不服輸`的我，最終`看到`了這篇文章 [[Day 4] Xcode安裝 為什麼有5000觀看??? ithelp觀察實驗](https://ithelp.ithome.com.tw/articles/10217609)，覺得感受到些許`安慰`還有想`偷笑`，原來`同是天涯淪落人`。以上內容`致敬`他的`文體`，只是我還是`習慣`用`標點符號`。
![](https://i.imgur.com/7PXolFY.png)


### 合
話說回來，這次比賽原本就是為了記錄我在[台灣好厲駭導師制度](https://isip.moe.edu.tw/wordpress/?page_id=368)的學習心得。而且在寫文章的過程中，時常會為了想把一項技術解釋得更精確而去查詢更多的相關資料。在這反覆的過程中，其實自己也能學習到很多東西。

因此如果問我參加鐵人賽到底值不值得，我的答案是肯定的；要是問我明年還要不要參加，再...再說。

![](https://i.imgur.com/EmUTyHg.png)



## 瀏覽數分析
以前只要刷新網頁就會算一次，後來小財神十分友善的回復，不久後就把這個問題修復了。
![](https://i.imgur.com/VZiVLIc.png)


### 目前規則
1. 如果帳號有成功登入，Request 有帶 ithelp2016_desktop 這個 Cookie，不管 IP 為何，每五分鐘只會算一次瀏覽數。
2. 如果是匿名瀏覽，這時則是看 IP，用幾個 IP 匿名瀏覽就會算幾次瀏覽數，一樣五分鐘重置一次。

### 刷瀏覽數
根據目前規則，有兩個可行方案來刷瀏覽數。

1. 創很多帳號，每五分鐘瀏覽一次目標文章。一天有 1440 分鐘，每 5 分鐘算一次，所以每個帳號一天最多能在同一篇文章創造 288 個瀏覽數。
2. 想辦法取得 IP，用不同 IP 匿名瀏覽目標文章。同樣的，每個 IP 一天最多能在同一篇文章創造 288 個匿名瀏覽數。

以 Security 這個主題而言，只要有 5 個 IP 或帳號，刷一天就可以超過第一名文章的瀏覽數。
![](https://i.imgur.com/QywWgpX.png)



### 建議
參考 [YouTube 計次方式](https://www.tech-girlz.com/2021/07/youtube-view-count-analysis.html)，我認為可以在現有的規則上再加入一些限制。如此一來，至少透過一般的機器人是無法大量產生瀏覽數的。
1. 確認使用者在文章連結的停留時間，可以根據文章的長度規定至少要停留多久才計算瀏覽數。
2. 根據使用者的游標位置(cursor position)、滑鼠滾輪(scroll) 等等因素判斷是否是真人正在瀏覽網頁。



## 最後的最後
希望這個網站可以變得越來越好，我本身也從這裡學到許多知識，也讀了很多不錯的文章來解決遇到的問題。

所有文章跟對應的專案都有備份到我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome)。

**[走囉～高歌離席 !](https://www.youtube.com/watch?v=6wouFCeF7AI)**

