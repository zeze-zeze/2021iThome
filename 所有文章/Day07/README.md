# 【Day 07】歡迎來到實力至上主義的 Shellcode (上) - Windows x86 Shellcode

## 環境
* Windows 10 21H1
* Visual Studio 2019
* NASM 2.14.02

## Shellcode 用途
Shellcode 是一段用於利用軟體漏洞而執行的代碼，藉由塞入一段可以讓 CPU 執行的機械碼，使電腦可以執行攻擊者的任意指令。有接觸過 CTF 的 Pwn 的人一定都很熟悉 Shellcode，當已經可以控制有漏洞的程式流程後，讓程式執行 Shellcode 就可以拿到 Shell。

在這一篇我將會說明 32-bit Windows Shellcode，執行 `WinExec("C:\Windows\System32\calc.exe", 10)`，以及介紹一些在 Windows 上寫 Shellcode 時會使用到的工具。雖然這篇是介紹 32-bit Shellcode，但是其實與 64-bit 概念大多相同。

## Linux Shellcode VS Windows Shellcode
應該有不少人學習是從 Linux 開始學習 Shellcode 的，所以這邊來比較一下兩者的差異。首先看看 x86 Linux 的 Shellcode，裡面使用的 `int 0x80` 是一個 Interrupt，而 0x80 是由 Kernel 處理的，可以用來讓程式呼叫 System Call。
```
; execve("/bin/sh", 0, 0)
0:  31 c0                   xor    eax,eax
2:  50                      push   eax
3:  68 2f 2f 73 68          push   0x68732f2f
8:  68 2f 62 69 6e          push   0x6e69622f
d:  89 e3                   mov    ebx,esp
f:  50                      push   eax
10: 50                      push   eax
11: 53                      push   ebx
12: 89 e1                   mov    ecx,esp
14: b0 0b                   mov    al,0xb
16: cd 80                   int    0x80
```

然而 Windows 不像 Linux 一樣可以直接使用 system call，而是要使用 Windows API，然後 Windows API 再去 call 那些 Native API。這些原因導致撰寫 Windows Shellcode 的步驟會變得比 Linux Shellcode 複雜許多，因為我們必須深入了解 PE 結構，從目標 dll 中取出需要的 Windows API 使用。

## 概略流程
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
### 1. 找到 kernel32.dll
#### 取得 PEB 位址
要得到 PEB 位址，首先要知道 [TIB(Thread Information Block)](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block)，它是用來存放有關目前的 Thread 的資訊，結構有點大可以自己點進連結看一下。

在 32-bit 和 64-bit，分別可以使用 FS、GS 這兩個 Segment Register 找到，而在 32-bit 我們的目標 PEB 就位在 FS:[0x30]，也就是 TIB 的 Offset 0x30。

#### 取得 PEB_LDR_DATA 位址
首先來看一下 [PEB 結構](https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb)
```
typedef struct _PEB {
  BYTE                          Reserved1[2];
  BYTE                          BeingDebugged;
  BYTE                          Reserved2[1];
  PVOID                         Reserved3[2];
  PPEB_LDR_DATA                 Ldr;
  PRTL_USER_PROCESS_PARAMETERS  ProcessParameters;
  PVOID                         Reserved4[3];
  PVOID                         AtlThunkSListPtr;
  PVOID                         Reserved5;
  ULONG                         Reserved6;
  PVOID                         Reserved7;
  ULONG                         Reserved8;
  ULONG                         AtlThunkSListPtr32;
  PVOID                         Reserved9[45];
  BYTE                          Reserved10[96];
  PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
  BYTE                          Reserved11[128];
  PVOID                         Reserved12[1];
  ULONG                         SessionId;
} PEB, *PPEB;
```

在第五個參數，也就是 PEB 的 Offset 0xC，就是我們的目標 PEB_LDR_DATA。

#### 取得 InMemoryOrderModuleList 位址
在 [PEB_LDR_DATA](http://undocumented.ntinternals.net/index.html?page=UserMode%2FStructures%2FPEB_LDR_DATA.html) 這個結構中可以看到三個 List，分別是 InLoadOrderModuleList、InMemoryOrderModuleList、InInitializationOrderModuleList，它們都是目前載入的 Image 的 Double Linked List，只是順序不同。
```
typedef struct _PEB_LDR_DATA {
  ULONG                   Length;
  BOOLEAN                 Initialized;
  PVOID                   SsHandle;
  LIST_ENTRY              InLoadOrderModuleList;
  LIST_ENTRY              InMemoryOrderModuleList;
  LIST_ENTRY              InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;
```

這三個 List 都是 [LIST_ENTRY](https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb_ldr_data) 結構，同時也是 Double Linked List 結構，Flink 指向下一個 Image，Blink 指向上一個 Image。
```
typedef struct _LIST_ENTRY {
   struct _LIST_ENTRY *Flink;
   struct _LIST_ENTRY *Blink;
} LIST_ENTRY, *PLIST_ENTRY, *RESTRICTED_POINTER PRLIST_ENTRY;
```


#### 取得 kernel32.dll 的 Base Address
那我們要這些 List 做什麼呢？如同上述，這三個 List 都是目前載入的 Image 的 Double Linked List，也就是說目標 kernel32.dll 也在其中。在這裡我們為了實作方便使用 InMemoryOrderModuleList，不然其實也可以迴圈任意一個 List 並透過對應 DLL 名稱來找到目標 DLL。

InMemoryOrderModuleList 是根據記憶體的載入順序排序，它的第三個 DLL 就是 kernel32.dll。其中在 List 的每個 Item 都指向一個 [_LDR_DATA_TABLE_ENTRY](https://www.geoffchappell.com/studies/windows/km/ntoskrnl/inc/api/ntldr/ldr_data_table_entry.htm) 結構。
```
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID      DllBase;
    PVOID      EntryPoint;
    ULONG32    SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    UINT32   Unknow[17];
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;
```

所以我們只要訪問三次 InMemoryOrderModuleList 的 Flink 就會到達 kernel32.dll 的 _LDR_DATA_TABLE_ENTRY 結構。注意這邊指向的位址是從 _LDR_DATA_TABLE_ENTRY+0x8 開始算，因為現在是用 InMemoryOrderModuleList 找，也就是說 DLLBase 在 Flink+0x10 位址。

最後取得的 DllBase 的位址就是 kernel32.dll 的 Base Address。

## POC
主要是參考 [Basics of Windows shellcode writing](https://idafchev.github.io/exploit/2017/09/26/writing_windows_shellcode.html#resources)，有改一點東西以符合 NASM 還有加一些註解。在我的 GitHub [zeze-zeze/2021iThome](https://github.com/zeze-zeze/2021iThome/tree/master/%E6%AD%A1%E8%BF%8E%E4%BE%86%E5%88%B0%E5%AF%A6%E5%8A%9B%E8%87%B3%E4%B8%8A%E4%B8%BB%E7%BE%A9%E7%9A%84Shellcode) 可以找到完整版的 POC。
```
; 1. 找到 Kernel32.dll
; 取得 PEB 位址
mov ebx, [fs:30h]

; 取得 PEB_LDR_DATA 位址
mov ebx, [ebx + 0x0C]

; 取得並訪問 InMemoryOrderModuleList 位址
mov ebx, [ebx + 0x14]

; 取得 kernel32.dll 的 Base Address
mov ebx, [ebx]
mov ebx, [ebx]
mov ebx, [ebx + 0x10] ; InMemoryOrderModuleList 的第三個 DLL 就是 kernel32.dll
```

## NASM
能把組語編譯成機械碼的工具有很多，這邊使用 [NASM](https://www.davidgrantham.com/)。
它支援各種平台包含 Windows、Linux，用法也很簡單。以我們現在要編的目標為例，因為是 32-bit Shellcode，所以 `nasm -f win32 shellcode.asm -o shellcode.obj` 就會產生 obj 副檔名的檔案。

看一下檔案類型 `Intel 80386 COFF object file, not stripped, 1 section, symbol offset=0xfe, 10 symbols` 是個 COFF object file，然而我們要的是純粹的 Shellcode，不需要其他東西。

這裡我示範用 IDA 取出目標 Shellcode，載入檔案後大概如下圖。
![](https://i.imgur.com/fX5ISOY.png)

雖然整個檔案大小為 192 Bytes，但是我們實際上需要的 Shellcode 就只有 0x14 Bytes，所以使用 [【Day 05】你逆 - 逆向工程工具介紹](https://ithelp.ithome.com.tw/articles/10268000)中推薦的 [LazyIDA](https://github.com/L4ys/LazyIDA) Plugin 按右鍵 => Convert => Convert to string 就可以轉成 C String 了。
![](https://i.imgur.com/fCDH92I.png)

## 參考資料
* [Basics of Windows shellcode writing](https://idafchev.github.io/exploit/2017/09/26/writing_windows_shellcode.html#resources)
* [Windows x64 Shellcode](https://mcdcyber.wordpress.com/2011/01/11/windows-x64-shellcode/)
* [Thread Information Block](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block)
* [NASM](https://www.davidgrantham.com/)
