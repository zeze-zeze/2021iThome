#include <intrin.h>
#include <Windows.h>
#pragma intrinsic(_ReturnAddress)

void PatchInt3() {
    PVOID pRetAddress = _ReturnAddress();

    // 如果 Return Address 是 int 3 (0xcc)，就改掉
    if (*(PBYTE)pRetAddress == 0xCC) {
        DWORD dwOldProtect;
        if (VirtualProtect(pRetAddress, 1, PAGE_EXECUTE_READWRITE, &dwOldProtect))
        {
            // 這邊是填成 nop(0x90)，在這個 POC 不會  Crash。
            // 但是最好還是改成原本 Return Address 的值，否則可能會 Crash
            *(PBYTE)pRetAddress = 0x90;
            VirtualProtect(pRetAddress, 1, dwOldProtect, &dwOldProtect);
        }
    }
}
int main(int argc, char* argv[]) {
    PatchInt3();
    MessageBoxW(0, L"You cannot keep debugging", L"Give up", 0);
}