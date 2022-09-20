    org    0x7c00            ; 告诉编译器程序加载到7c00处
    mov    ax, cs
    mov    ds, ax
    mov    es, ax
    call   ClearScreen       ; cls
    call   DispScreen        ; output NWPU
    call   ResetCursor       ; reset cursor
    jmp    $                 ; 无限循环
ClearScreen:
    mov     ah, 0x06
    mov     al, 0
    mov     bh, 0x0f
    mov     cx, 0
    mov     dx, 0xffff
    int     10h
    ret
DispScreen:
    mov    ax, BootMessage
    mov    bp, ax            ; ES:BP = 串地址
    mov    ax, 0x1300        ; AH = 13,  AL = 01h
    mov    bx, 0x00f9        ; 页号为0(BH = 0) 黑底红字(BL = 0Ch,高亮)
    mov    cx, 4             ; CX = 串长度
    mov    dx, 0x1326
    int    10h               ; 10h 号中断
    ret
ResetCursor:
    mov     ah, 0x02
    mov     bh, 0
    mov     dx, 0x0000
    int     10h
    ret
BootMessage:             db    "NWPU"	; some change
times      510-($-$$)    db    0    ; 填充剩下的空间，使生成的二进制代码恰好为512字节
dw         0xaa55                   ; 结束标志

call CodeA
call CodeB

CodeA:
	…
	ret

CodeB:
	…
	ret
