.CODE
SetTrapFlag PROC
    ; 把 Flag 們丟到 Stack 上
	pushf

    ; 設定 Trap Flag，位置在第 9 個 bit，所以是 0x100
    or dword ptr [rsp], 100h

    ; 把新的 Flag 從 Stack 拿給 Flag 們
    popf
    nop
    ret
SetTrapFlag ENDP
END