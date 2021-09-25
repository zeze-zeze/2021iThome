#include <windows.h>
#include <string>
#include <psapi.h>

int main(int argc, char* argv[]) {
    // 1. 開啟目標 Process (iexplore.exe 的 pid)
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 5548);
    if (!hProcess) {
        printf("OpenProcess failed: error %d\n", GetLastError());
        return 1;
    }

    // 2. 用 EnumProcessModules 取得所有的 Module Handle
    HMODULE hMods[1024], hModule = NULL;
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (int i = 0; i < (int)(cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModPathName[MAX_PATH] = { 0 };

            // 3. 用 GetModuleFileNameEx 取得目前的 Module Name
            if (GetModuleFileNameEx(hProcess, hMods[i], szModPathName, sizeof(szModPathName) / sizeof(TCHAR))) {
                printf("%ls\n", szModPathName);

                // 4. 判斷是不是目標 (WININET)，是的話就記錄下來
                std::wstring sMod = szModPathName;
                if (sMod.find(L"WININET") != std::string::npos) {
                    hModule = hMods[i];
                }
            }
            else {
                printf("GetModuleFileNameEx failed: error %d\n", GetLastError());
                return NULL;
            }
        }
    }
    else {
        printf("EnumProcessModulesEx failed: error %d\n", GetLastError());
        return 1;
    }
    if (hModule == NULL) {
        printf("Cannot find target module\n");
        return 1;
    }
    printf("\n\nWININET.dll handle: %p\n", hModule);

    // 5. 用 GetModuleInformation 取得 Module 資訊
    MODULEINFO lpmodinfo;
    if (!GetModuleInformation(hProcess, hModule, &lpmodinfo, sizeof(MODULEINFO))) {
        printf("GetModuleInformation failed: error %d\n", GetLastError());
        return 1;
    }
    return 0;
}