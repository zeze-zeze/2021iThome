# 【Day 12】卑鄙源之 Hook (下) - 偵測 Hook

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在[【Day 11】卑鄙源之 Hook (上) - 偵測 Hook](https://ithelp.ithome.com.tw/articles/10272274) 我們提到可以比對檔案與記憶體來判斷函數是否有被 Hook，也透過幾個 Windows API 把目標 Process iexplore.exe 中的目標 Module WININET.DLL 找出來並取得 Handle。

這一篇要來處理檔案的部分，同樣找到檔案中的 WININET.DLL，然後比對檔案與記憶體的差異，透過兩者一不一樣判斷有沒有被 Hook。

## 將檔案內容 Map 到 Process 中
首先我們要知道 WININET.DLL 的完整路徑是 `C:\Windows\SysWOW64\wininet.dll`，注意這邊不是 `C:\Windows\System32\wininet.dll`，因為之前實作的目標是 32-bit Process。

既然已經知道檔案路徑，接下來只要
1. 取得檔案的 Handle
2. 建立 Mapping 物件
3. 把檔案內容載到當前的 Process 中

這三步分別對應到三個函數，[CreateFile](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea)、[CreateFileMapping](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-createfilemappingw)、[MapViewOfFile](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile)，如此就可以讀取檔案內容。

## 讀取 PE 的結構找到目標 Export Function
### RVA
目前已經可以讀取檔案與記憶體的內容，接下來要找到目標 Function HttpSendRequestW 的 RVA。

RVA 全名為 Relative Virtual Address，也就是與 Image Base 的距離。假設目前 wininet.dll 的 Image Base 是 0x71280000，HttpSendRequestW 的位址是 0x7159B7C0，則 HttpSendRequestW 的 RVA 就是 `0x7159B7C0 - 0x71280000 = 0x31B7C0`。

這邊要注意的是 RVA 不等於 File Offset 哦，`File Offset = RVA - Virtual Offset + Raw Offset`。

### PE 結構
PE 結構的部分其實已經在 [【Day 07】歡迎來到實力至上主義的 Shellcode (上) - Windows x86 Shellcode](https://ithelp.ithome.com.tw/articles/10269530)、[【Day 08】歡迎來到實力至上主義的 Shellcode (下) - Windows x86 Shellcode](https://ithelp.ithome.com.tw/articles/10270253) 解釋過，但是這次是要用 C/C++ 訪問 PE 結構，所以這邊再快速說明一次。

首先，Image Base 的位址就是 [IMAGE_DOS_HEADER](https://www.nirsoft.net/kernel_struct/vista/IMAGE_DOS_HEADER.html) 的開頭，IMAGE_DOS_HEADER 的結構如下。可以透過其中的 e_lfanew 算出 [IMAGE_NT_HEADERS](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_nt_headers32)，`IMAGE_NT_HEADERS = IMAGE_DOS_HEADER + e_lfanew`
```
typedef struct _IMAGE_DOS_HEADER
{
     WORD e_magic;
     WORD e_cblp;
     WORD e_cp;
     WORD e_crlc;
     WORD e_cparhdr;
     WORD e_minalloc;
     WORD e_maxalloc;
     WORD e_ss;
     WORD e_sp;
     WORD e_csum;
     WORD e_ip;
     WORD e_cs;
     WORD e_lfarlc;
     WORD e_ovno;
     WORD e_res[4];
     WORD e_oemid;
     WORD e_oeminfo;
     WORD e_res2[10];
     LONG e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;
```

IMAGE_NT_HEADERS 的結構如下。在這裡我們需要的是 [OptionalHeader](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32)，因為它可以幫助我們找到 [IMAGE_DATA_DIRECTORY](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_data_directory) 結構，再利用其中的 VirtualAddress 算出 IMAGE_EXPORT_DIRECTORY。
```
typedef struct _IMAGE_NT_HEADERS {
  DWORD                   Signature;
  IMAGE_FILE_HEADER       FileHeader;
  IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;
```

找到 IMAGE_EXPORT_DIRECTORY 的用意是為了找到重要的三個 Table，分別是 Function Table、Ordinal Table、Name Table，IMAGE_EXPORT_DIRECTORY 結構如下，三個 Table 的 RVA 分別是 AddressOfFunctions、AddressOfNames、AddressOfNameOrdinals。

Name Table 中存放著所有 Export Function 名稱字串的 RVA，找到目標函數名稱後，拿對應的 Name Table 的 Index 去找 Ordinal Table。再把 Ordinal Table 對應的值當作 Index 去找 Function Table，得到的值就是目標 Function 的 RVA。
```
typedef struct _IMAGE_EXPORT_DIRECTORY {
    DWORD   Characteristics;
    DWORD   TimeDateStamp;
    WORD    MajorVersion;
    WORD    MinorVersion;
    DWORD   Name;
    DWORD   Base;
    DWORD   NumberOfFunctions;
    DWORD   NumberOfNames;
    DWORD   AddressOfFunctions;     // RVA from base of image
    DWORD   AddressOfNames;         // RVA from base of image
    DWORD   AddressOfNameOrdinals;  // RVA from base of image
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;
```

## 實作
### 實作流程
1. 把 wininet.dll 檔案 Map 到目前的 Process 中
    * 取得檔案的 Handle
    * 建立 Mapping 物件
    * 把檔案內容載到當前的 Process 中
2. 讀取 wininet.dll 檔案的 PE 結構，取得 HttpSendRequestW 的 RVA
    * 取得 IMAGE_DOS_HEADER 結構後，接著一直找其他 Header，直到找出 IMAGE_EXPORT_DIRECTORY
    * 從 IMAGE_EXPORT_DIRECTORY 找出 Function Table、Ordinal Table、Name Table
    * 對 Name Table 迴圈找到 HttpSendRequestW，找到後透過 Function Table、Ordinal Table 取得 RVA
3. 比對檔案與 iexplore.exe 的 HttpSendRequestW 是否相同
    * 用 ReadProcessMemory 讀取 iexplore.exe 的 HttpSendRequestW 函數的前 5 Bytes
    * 用 memcmp 比對檔案和記憶體的 HttpSendRequestW 一不一樣

### POC
完整的程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E5%8D%91%E9%84%99%E6%BA%90%E4%B9%8BHook/DetectHook)。
```cpp=
#include <windows.h>
#include <string>
#include <psapi.h>

int main(int argc, char* argv[]) {
    // 開啟目標 Process (iexplore.exe 的 pid)
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 17704);
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
    /* 以上是卑鄙源之 Hook (上) 的內容 */

    /* 以下是卑鄙源之 Hook (下) 的內容 */
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
```


### 實際測試
記得把 pid 換成自己環境測試的 iexplore.exe 的 pid。

拿之前做的 Hook IE 的 POC 來測試，在 Hook 之前，程式應該會輸出 `Not Hook`；在執行 Hook 之後，應該會輸出 `Hook`。

## 道高一尺，魔高一丈
可能有些人馬上就想到了繞過方法，因為這篇給的 POC 只檢查了函數的前 5 Bytes，所以只要把 Hook 設在中間，這篇給的 POC 不就沒用了嗎。沒錯，這招就是 Mid Function Hook，拿這招去繞 GitHub 專案 [HookHunter](https://github.com/mike1k/HookHunter) 與 PCHunter 也能成功，因為大部分情況都只會檢查前幾個 Bytes。

但是 Mid Function Hook 會提高紅隊惡意程式的開發成本與降低程式穩定性，而且籃隊也可能選擇犧牲效能檢查整個函數。所以 Hook 與偵測 Hook 就感覺演變成像是一種貓捉老鼠的遊戲。
