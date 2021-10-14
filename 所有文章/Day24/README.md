# 【Day 24】上百種 Provider 任意選，這樣的 ETW 你喜歡嗎 - ETW 監控 Process

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在[【Day 23】為美好的 Windows 獻上 ETW - Event Tracing for Windows](https://ithelp.ithome.com.tw/articles/10279093) 提到對於藍隊而言，ETW 是個有效監控程式行為的方式。最後也有使用 Windows 內建工具 - logman 進行 Process 的監控，然而 logman 只是單純將事件寫入檔案，無法彈性的分析抓取的事件，因此這一篇將會介紹如何自己寫程式利用 ETW 抓取事件並進行分析。

在講解 ETW 時，有說 ETW 是由 Controller、Consumer、Provider 三個角色組成，這篇使用的 Provider 是 Windows 內建的 Microsoft-Windows-Kernel-Process。

## 準備工作
首先，Microsoft 有開發一個開源專案 - [KrabsETW](https://github.com/microsoft/krabsetw)，可以很方便的建立 Session 收取事件。這篇主要會使用這個專案寫 POC。

當然這個專案的原理也是呼叫 Windows API，不過 KrabsETW 將這個流程簡化並統整，如果有想要直接學習原始的 API 怎麼用的，可以參考 [MSDN 的範例](https://docs.microsoft.com/en-us/windows/win32/etw/using-events-to-calculate-cpu-usage)。

不常碰 Windows 開發的朋友可以先建一下 [vcpkg](https://github.com/microsoft/vcpkg) 的環境。這篇會用到的 Library 有兩個，分別是剛剛提到的 KrabsETW 和 JSON。
```
vcpkg.exe install nlohmann-json:x64-windows
vcpkg.exe install krabsetw:x64-windows
```

## 任務講解
這次使用的 Provider 是 Microsoft-Windows-Kernel-Process，GUID 為 `{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}`。用上一篇提到的工具 - logman 看一下詳細資訊。
```
# logman query providers Microsoft-Windows-Kernel-Process

Provider                                 GUID
-------------------------------------------------------------------------------
Microsoft-Windows-Kernel-Process         {22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}

Value               Keyword              Description
-------------------------------------------------------------------------------
0x0000000000000010  WINEVENT_KEYWORD_PROCESS
0x0000000000000020  WINEVENT_KEYWORD_THREAD
0x0000000000000040  WINEVENT_KEYWORD_IMAGE
0x0000000000000080  WINEVENT_KEYWORD_CPU_PRIORITY
0x0000000000000100  WINEVENT_KEYWORD_OTHER_PRIORITY
0x0000000000000200  WINEVENT_KEYWORD_PROCESS_FREEZE
0x0000000000000400  WINEVENT_KEYWORD_JOB
0x0000000000000800  WINEVENT_KEYWORD_ENABLE_PROCESS_TRACING_CALLBACKS
0x0000000000001000  WINEVENT_KEYWORD_JOB_IO
0x0000000000002000  WINEVENT_KEYWORD_WORK_ON_BEHALF
0x0000000000004000  WINEVENT_KEYWORD_JOB_SILO
0x8000000000000000  Microsoft-Windows-Kernel-Process/Analytic

Value               Level                Description
-------------------------------------------------------------------------------
0x04                win:Informational    Information

PID                 Image
-------------------------------------------------------------------------------
0x00000000


The command completed successfully.
```

一個 Provider 可能會提供多種事件，每個事件又有對應的 Event ID。每個 Event ID 的版本會提供的資料欄位已經有人整理在 [Windows10EtwEvents](https://github.com/jdu2600/Windows10EtwEvents/blob/master/manifest/Microsoft-Windows-Kernel-Process.tsv)。這一篇的目標會放在 Event ID 1 的事件，也就是 ProcessStart。

## 實作
### 實作流程
1. 建立 Session
2. 設定 Provider 資訊
3. 設置 Callback 處理事件，其中會把欄位 Parse 好
4. 設定 Session 從目標 Provider 抓取事件
5. 啟用 Session

### POC
程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E4%B8%8A%E7%99%BE%E7%A8%AEProvider%E4%BB%BB%E6%84%8F%E9%81%B8%E9%80%99%E6%A8%A3%E7%9A%84ETW%E4%BD%A0%E5%96%9C%E6%AD%A1%E5%97%8E)。
```cpp=
#include <krabs.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>

// ws2s: 將型別 wstring 轉 string
std::string ws2s(const std::wstring& wstr) {
	const std::string str(wstr.begin(), wstr.end());
	return str;
}

// byte2uint32: 將型別 vector<Byte> 轉為 uint32
uint32_t byte2uint32(std::vector<BYTE> v) {
    uint32_t res = 0;
    for (int i = (int)v.size() - 1; i >= 0; i--) {
        res <<= 8;
        res += (uint32_t)v[i];
    }
    return res;
}

// byte2uint64: 將型別 vector<BYTE> 轉為 uint64
uint64_t byte2uint64(std::vector<BYTE> v) {
    uint64_t res = 0;
    for (int i = 7; i >= 0; i--) {
        res <<= 8;
        res += (uint64_t)v[i];
    }
    return res;
}

// callback: 當 Session 從 Provider 拿到任何事件時會呼叫
void callback(const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
    // 取得事件模型並 Parse
    krabs::schema schema(record, trace_context.schema_locator);
    krabs::parser parser(schema);
    
    // 如果是 ProcessStart 事件才處理
    if (schema.event_id() == 1) {
        // 將事件中的每個欄位 Parse 後轉成需要的型別並存放到 json 中
        nlohmann::json data;
        data["ProcessID"] = byte2uint32(parser.parse<krabs::binary>(L"ProcessID").bytes());
        data["CreateTime"] = byte2uint64(parser.parse<krabs::binary>(L"CreateTime").bytes());
        data["ParentProcessID"] = byte2uint32(parser.parse<krabs::binary>(L"ParentProcessID").bytes());
        data["SessionID"] = byte2uint32(parser.parse<krabs::binary>(L"SessionID").bytes());
        data["ImageName"] = ws2s(parser.parse<std::wstring>(L"ImageName"));
        
        // 將 json 資料印出，參數 4 讓印出來的資料比較容易看
        std::cout << data.dump(4) << std::endl;
    }
}

int main(int argc, const char* argv[]) {
    // 1. 建立 Session
    krabs::user_trace session(L"ETW_example");
    
    // 2. 設定 Provider 資訊
    krabs::provider<> provider(L"Microsoft-Windows-Kernel-Process");
    provider.any(0x10); // 這是上一篇講解過的 flags
    
    // 3. 設置 Callback 處理事件
    provider.add_on_event_callback(callback);

    // 4. 設定 Session 從目標 Provider 抓取事件
    session.enable(provider);

    // 5. 啟用 Session
    session.start();
}
```

### 實際測試
在執行 POC 後，Cmd 應該會掛著，將執行的 Process 資訊印出來。以執行 explorer.exe 為例，執行結果應大致如下，實際輸出會根據系統狀態不同而改變
```
{
    "CreateTime": 132744674520123800,
    "ImageName": "\\Device\\HarddiskVolume5\\Windows\\explorer.exe",
    "ParentProcessID": 17640,
    "ProcessID": 14236,
    "SessionID": 2
}
```

## 參考資料
* [microsoft/krabsetw](https://github.com/microsoft/krabsetw)
