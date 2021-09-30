#include <Windows.h>

int main(int argc, char* argv[]) {
    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // 取得 Context 結構
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;

    // 確認 DR0~DR3 有沒有被設定，有非 0 值代表有 Hardware Breakpoint
    if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) {
        MessageBoxW(0, L"Detect", L"Hardware Breakpoint", 0);
    }
    else {
        MessageBoxW(0, L"Not Detect", L"Hardware Breakpoint", 0);
    }
}
