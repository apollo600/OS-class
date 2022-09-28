org	0400h
	
	; 使用偏移量的方式将bootmessage中的数据写入al
	; 然后指定对应的行和列，列需要加上字符串的偏移量
	mov ax, .BootMessage
	mov bp, ax
	mov cx, 26	; 字符串输出次数 26
	mov dh, 2	;  3 行
	mov dl, 27	; 27 列

.DispChar:
	; 计算下标存储到ax先
	mov	ax, 0B800h
	mov	gs, ax
	mov	ax, 0x50	; \80
	mov bl, dh		; |* 行号
	mul bl
	mov bl, dl
	add al, bl		; |+ 列号		
	mov bl, 0x02
	mul bl
	mov bx, ax
	mov	ah, 0x0c	
	mov	al, es:bp
	mov	[gs:(bx)], ax	; 输出到屏幕

	inc dl
	inc bp
	dec cx
	cmp cx, 0
	jz	.DispFinished
	jmp	.DispChar

.DispFinished:
	jmp $

.BootMessage: db "This is mengxiangyu's boot"
