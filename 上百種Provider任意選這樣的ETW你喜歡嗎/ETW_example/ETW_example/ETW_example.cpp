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