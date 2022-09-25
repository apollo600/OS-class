org	0400h

	mov	ax, 0B800h
	mov	gs, ax
	mov	ah, 0Fh				; 0000: 黑底    1111: 白字
	
	; 使用偏移量的方式将bootmessage中的数据写入al
	; 然后指定对应的行和列，列需要加上字符串的偏移量
	mov ax, .BootMessage
	mov bp, ax
	mov cx, 6	; 字符串输出次数
	mov dh, 2	; 行
	mov dl, 39	; 列

	; 计算下标存储到ax先
	mov	ax, 0x50
	mul dh
	add ax, dl
	mul 0x02
	mov di, ax
	mov	ah, 0Fh	
	mov	al, ES:[BP]
	mov	[gs:di], ax	; 屏幕第 0 行, 第 39 列
	
	; inc BP
	; mov	ah, 0Fh	
	; mov	al, ES:[BP]
	; mov	[gs:((80 * 2 + 39) * 2)], ax	; 屏幕第 0 行, 第 39 列
.BootMessage: db "LOADER"

	jmp	$				; 到此停住
