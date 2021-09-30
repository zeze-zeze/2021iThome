#include <Windows.h>

extern "C" void SetTrapFlag();

int main(int argc, char* argv[]) {
    __try {
        // 如果是在 Debugger 中執行這個 Function，
        // 觸發的 Exception 卻沒有被自己定義的意外處理方式處理，
        // 表示這個 Exception 已經被 Debugger 處理了，也就代表目前正在被 Debug
        SetTrapFlag();
        MessageBoxW(0, L"Detect", L"Debugger", 0);
    }
    __except (GetExceptionCode() == EXCEPTION_SINGLE_STEP
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_EXECUTION) {
        MessageBoxW(0, L"Not Detect", L"Debugger", 0);
    }
}
