#include <windows.h>
#include <string>
#include <psapi.h>

int main(int argc, char* argv[]) {
    // 開啟目標 Process (iexplore.exe 的 pid)
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 13952);
    if (!hProcess) {
        printf("OpenProcess failed: error %d\n", GetLastError());
        return 1;
    }

    // 用 EnumProcessModules 取得所有的 Module Handle
    HMODULE hMods[1024], hModule = NULL;
    DWORD cbNeeded;
    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (int i = 0; i < (int)(cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModPathName[MAX_PATH] = { 0 };

            // 用 GetModuleFileNameEx 取得目前的 Module Name
            if (GetModuleFileNameEx(hProcess, hMods[i], szModPathName, sizeof(szModPathName) / sizeof(TCHAR))) {
                // 判斷是不是目標 (WININET)，是的話就記錄下來
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

    // 用 GetModuleInformation 取得 Module 資訊
    MODULEINFO lpmodinfo;
    if (!GetModuleInformation(hProcess, hModule, &lpmodinfo, sizeof(MODULEINFO))) {
        printf("GetModuleInformation failed: error %d\n", GetLastError());
        return 1;
    }
    /* 以上是湯姆與傑利 (上) 的內容 */

    /* 以下是湯姆與傑利 (下) 的內容 */
    // 1. 把 wininet.dll 檔案 Map 到目前的 Process 中
    // 取得檔案的 Handle
    HANDLE hFile = CreateFile(L"C:\\Windows\\SysWOW64\\wininet.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("CreateFile failed: error %d\n", GetLastError());
        return NULL;
    }

    // 建立 Mapping 物件
    HANDLE file_map = CreateFileMapping(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, L"KernelMap");
    if (!file_map) {
        printf("CreateFileMapping failed: error %d\n", GetLastError());
        return NULL;
    }

    // 把檔案內容載到當前的 Process 中
    LPVOID file_image = MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0);
    if (file_image == 0) {
        printf("MapViewOfFile failed: error %d\n", GetLastError());
        return NULL;
    }

    // 2. 讀取 wininet.dll 檔案的 PE 結構，取得 HttpSendRequestW 的 RVA
    DWORD RVA = 0;

    // 取得 IMAGE_DOS_HEADER 結構後，接著一直找其他 Header，直到找出 IMAGE_EXPORT_DIRECTORY
    PIMAGE_DOS_HEADER pDos_hdr = (PIMAGE_DOS_HEADER)file_image;
    PIMAGE_NT_HEADERS pNt_hdr = (PIMAGE_NT_HEADERS)((char*)file_image + pDos_hdr->e_lfanew);
    IMAGE_OPTIONAL_HEADER opt_hdr = pNt_hdr->OptionalHeader;
    IMAGE_DATA_DIRECTORY exp_entry = opt_hdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    PIMAGE_EXPORT_DIRECTORY pExp_dir = (PIMAGE_EXPORT_DIRECTORY)((char*)file_image + exp_entry.VirtualAddress);

    // 從 IMAGE_EXPORT_DIRECTORY 找出 Function Table、Ordinal Table、Name Table
    DWORD* func_table = (DWORD*)((char*)file_image + pExp_dir->AddressOfFunctions);
    WORD* ord_table = (WORD*)((char*)file_image + pExp_dir->AddressOfNameOrdinals);
    DWORD* name_table = (DWORD*)((char*)file_image + pExp_dir->AddressOfNames);

    // 對 Name Table 迴圈找到 HttpSendRequestW，找到後透過 Function Table、Ordinal Table 取得 RVA
    for (int i = 0; i < (int)pExp_dir->NumberOfNames; i++) {
        if (strcmp("HttpSendRequestW", (const char*)file_image + (DWORD)name_table[i]) == 0) {
            RVA = (DWORD)func_table[ord_table[i]];
        }
    }
    if (!RVA) {
        printf("Failed to find target function\n");
    }

    // 3. 比對檔案與 iexplore.exe 的 HttpSendRequestW 是否相同
    // 用 ReadProcessMemory 讀取 iexplore.exe 的 HttpSendRequestW 函數的前 5 Bytes
    TCHAR* lpBuffer = new TCHAR[6]{ 0 };
    if (!ReadProcessMemory(hProcess, (LPCVOID)((DWORD)lpmodinfo.lpBaseOfDll + RVA), lpBuffer, 5, NULL)) {
        printf("ReadProcessMemory failed: error %d\n", GetLastError());
        return -1;
    }

    // 用 memcmp 比對檔案和記憶體的 HttpSendRequestW 一不一樣
    if (memcmp((LPVOID)((DWORD)file_image + RVA), (LPVOID)((DWORD)lpBuffer), 5) == 0) {
        printf("Not Hook\n");
        return 0;
    }
    else {
        printf("Hook\n");
        return 1;
    }

    return 0;
}