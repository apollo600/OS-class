org	0400h

jmp Main
nop

; FAT12 磁盘的头
; ===================================================
BS_OEMName        DB 'ForrestY'     ; OEM String, 必须 8 个字节
BPB_BytsPerSec    DW 512            ; 每扇区字节数
BPB_SecPerClus    DB 1              ; 每簇多少扇区
BPB_RsvdSecCnt    DW 1              ; Boot 记录占用多少扇区
BPB_NumFATs       DB 2              ; 共有多少 FAT 表
BPB_RootEntCnt    DW 224            ; 根目录文件数最大值
BPB_TotSec16      DW 2880           ; 逻辑扇区总数
BPB_Media         DB 0xF0           ; 媒体描述符
BPB_FATSz16       DW 9              ; 每FAT扇区数
BPB_SecPerTrk     DW 18             ; 每磁道扇区数
BPB_NumHeads      DW 2              ; 磁头数(面数)
BPB_HiddSec       DD 0              ; 隐藏扇区数
BPB_TotSec32      DD 0              ; 如果 wTotalSectorCount 是 0 由这个值记录扇区数
BS_DrvNum         DB 80h            ; 中断 13 的驱动器号
BS_Reserved1      DB 0              ; 未使用
BS_BootSig        DB 29h            ; 扩展引导标记 (29h)
BS_VolID          DD 0              ; 卷序列号
BS_VolLab         DB 'OrangeS0.02'  ; 卷标, 必须 11 个字节
BS_FileSysType    DB 'FAT12   '     ; 文件系统类型, 必须 8个字节  

; ===================================================
; 按照 常量->变量->子函数->主函数的顺序给出
; 常量
; ===================================================
; boot的内存模型实际上比较简单（区间左闭右开）
; (0x500~0x7c00)     栈
; (0x7c00~0x7e00)    引导扇区
; (0x90000~0x90400)  缓冲区，GetNextCluster函数会用到它
; (0x90400~?)        加载区，loader代码会加载到这里
BaseOfStack               equ 07c00h  	; Boot状态下堆栈基地址(栈底, 从这个位置向低地址生长)
BaseOfLoader              equ 09000h  	; LOADER.BIN 被加载到的位置 ---- 段地址
OffsetOfLoader            equ 0400h   	; LOADER.BIN 被加载到的位置 ---- 偏移地址

RootDirSectors            equ 14		; 根目录扇区数
SectorNoOfRootDirectory   equ 19		; 根目录的扇区编号
SectorNoOfFAT1            equ 1			; FAT1表的起始扇区编号
DeltaSectorNo             equ 31		; 簇号+Delta=扇区号
; ===================================================
; 变量
; ===================================================
LoaderFileName            db    "LOADER  BIN", 0  ; LOADER.BIN 的文件名(为什么中间有空格请RTFM)
; 为简化代码, 下面每个字符串的长度均为 MessageLength
MessageLength             equ    9
BootMessage:              db    "Booting  "    ; 9字节, 不够则用空格补齐. 序号 0
Message1                  db    "Ready.   "    ; 9字节, 不够则用空格补齐. 序号 1
Message2                  db    "Read Fail"    ; 9字节, 不够则用空格补齐. 序号 2
Message3                  db    "No Loader"    ; 9字节, 不够则用空格补齐. 序号 3
Message4				  db	"Searching"	   ; 9字节, 不够则用空格补齐. 序号 4
Message5				  db	"ClusNum: "	   ; 9字节, 不够则用空格补齐. 序号 5

LeftRootDirSectors        dw    RootDirSectors          ; 还未搜索的根目录扇区数
RootDirSectorNow          dw    SectorNoOfRootDirectory ; 目前正在搜索的根目录扇区
BufferPacket              times 010h db 0
; ===================================================
; 子函数
; ===================================================

;----------------------------------------------------------------------------
; 函数名: DispStr
;----------------------------------------------------------------------------
; 作用:
;    显示一个字符串, 函数开始时 dh 中应该是字符串序号(从0开始)
DispStr:
    ; init stack
    push   bp
    mov    bp, sp
    pusha
    push   es

    ; push the string to the end of stack
    mov    ax, MessageLength
    mul    dh
    add    ax, BootMessage
    mov    bp, ax   

    ; get the string using %bp calculated above
    mov    ax, ds        
    mov    es, ax            ; ES:BP = 串地址
    mov    cx, MessageLength ; CX = 串长度
    mov    ax, 01301h        ; AH = 13,  AL = 01h
    mov    bx, 0007h         ; 页号为0(BH = 0) 黑底白字(BL = 07h)
    mov    dl, 0
    int    10h

    ; recover the stack
    pop    es
    popa
    pop    bp
    ret

;----------------------------------------------------------------------------
; 函数名: DispDot
;----------------------------------------------------------------------------
; 作用:
;    打印一个点
DispDot:
    push   bp
    mov    bp, sp
    pusha

    mov    ah, 0Eh       ; `. 每读一个扇区就在 "Booting  " 后面
    mov    al, '.'       ;  | 打一个点, 形成这样的效果:
    mov    bl, 0Fh       ;  | Booting ......
    int    10h           ; /

    popa
    pop    bp
    ret

; 函数名: ReadSector
;----------------------------------------------------------------------------
; 作用:
;    将磁盘的数据读入到内存中
;    ax: 从哪个扇区开始
;    cx: 读入多少个扇区
;    (es:bx): 读入的缓冲区的起始地址
;
;    中断调用传入的参数规范请参考本节实验指导书的实验参考LBA部分
ReadSector:
    push   bp
    mov    bp, sp
    pusha

    mov    si, BufferPacket      ; ds:si 指向的是BufferPacket的首地址
    mov    word [si + 0], 010h   ; buffer_packet_size
    mov    word [si + 2], cx     ; sectors
    mov    word [si + 4], bx     ; buffer-offset
    mov    word [si + 6], es     ; buffer-segment
    mov    word [si + 8], ax     ; start_sectors

    mov    dl, [BS_DrvNum]       ; 驱动号
    mov    ah, 42h               ; 扩展读
    int    13h
    jc     .ReadFail             ; 读取失败，简单考虑就默认bios坏了
    
    popa
    pop    bp
    ret

.ReadFail:
    mov    dh, 2
    call   DispStr
    jmp    $                     ; 如果cf位置1，就意味着读入错误，这个时候建议直接开摆

;----------------------------------------------------------------------------
; 函数名: GetNextCluster
;----------------------------------------------------------------------------
; 作用:
;    ax存放的是当前的簇(cluster)号，根据当前的簇号在fat表里查找，找到下一个簇的簇号，并将返回值存放在ax
GetNextCluster:
    push   bp
    mov    bp, sp
    pusha

    mov    bx, 3              ; 一个FAT项长度为1.5字节
    mul    bx
    mov    bx, 2              ; ax = floor(clus_number * 1.5)
    div    bx                 ; 这个时候ax里面放着的是FAT项基地址相对于FAT表开头的字节偏移量
                              ; 如果clus_number为奇数，则dx为1，否则为0
    push   dx                 ; 临时保存奇数标识信息
    mov    dx, 0              ; 下面除法要用到
    mov    bx, [BPB_BytsPerSec]
    div    bx                 ; dx:ax / BPB_BytsPerSec
                              ; ax <- 商 (基地址在FAT表的第几个扇区)
                              ; dx <- 余数 (基地址在扇区内的偏移)
    mov    bx, 0              ; bx <- 0 于是, es:bx = BaseOfLoader:0
    add    ax, SectorNoOfFAT1 ; 此句之后的 ax 就是FAT项所在的扇区号
    mov    cx, 2              ; 读取FAT项所在的扇区, 一次读两个, 避免在边界
    call   ReadSector         ; 发生错误, 因为一个FAT项可能跨越两个扇区

    mov    bx, dx             ; 将偏移量搬回bx
    mov    ax, [es:bx]
    pop    bx                 ; 取回奇数标识信息
    cmp    bx, 0              ; 如果是第奇数个FAT项还得右移四位
    jz     EvenCluster        ; 可能是微软(FAT是微软创建的)第一个亲儿子的原因，有它的历史局限性
    shr    ax, 4              ; 当时的磁盘很脆弱，经常容易写坏，所以需要两张FAT表备份，而且人们能够制作的存储设备的容量很小
EvenCluster:
    and    ax, 0FFFh          ; 读完需要与一下，因为高位是未定义的，防止ax值有误
    mov    word [bp - 2], ax  ; 这里用了一个技巧，这样在popa的时候ax也顺便更新了

    popa
    pop    bp
    ret

;----------------------------------------------------------------------------
; 函数名: StringCmp
;----------------------------------------------------------------------------
; 作用:
;    比较 ds:si 和 es:di 处的字符串（比较长度为11，仅为loader.bin所用）
;    如果两个字符串相等ax返回1，否则ax返回0
StringCmp:
    push   bp
    mov    bp, sp
    pusha

    mov    cx, 11                 ; 比较长度为11
    cld                           ; 清位保险一下
.STARTCMP:
    lodsb                         ; ds:si -> al
    cmp    al, byte [es:di]       ; same: ZF=1, diff:ZF=0
    jnz    .DIFFERENT
    inc    di                     ; -> di++
    dec    cx                     ; -> cx--
    cmp    cx, 0
    jz     .SAME
    jmp    .STARTCMP
.DIFFERENT:
    mov    word [bp - 2], 0     ; 这里用了一个技巧，这样在popa的时候ax也顺便更新了
    jmp    .ENDCMP
.SAME:
    mov    word [bp - 2], 1     ; 下一步就是ENDCMP了，就懒得jump了
.ENDCMP:
    popa
    pop    bp
    ret

MyDisp:  
    push   bp
    mov    bp, sp
    pusha
    
    ; 不使用bx, dx进行输出操作
    ; cx作为中转寄存器
    ; di作为列号
    and    al, 0x0f             ; 取寄存器后4位
    cmp    al, 9                
    jnc    .Char                ; 大于9,为字母

.Num:
    add    al, 0x30             ; 数字+48      
    jmp    .Out

.Char:
    add    al, 0x37             ; 字母+55
    jmp    .Out

.Out:
    mov    bx, 0xb800
    mov    gs, bx               ; 初始化gs地址
    mov    ah, 0x0c             ; 设置输出内容
    ; 计算gs偏移量
    ; (80*2+di)*2
    dec    di                   ; 减1转换一下
    mov    si, di
    add    si, 0x00a0           ; 160
    shl    si, 1                ; * 2
    mov    [gs:si], ax          ; 输出

.End:
    popa
    pop    bp
    ret
; ============================================= 

; ===================================================
; 主函数
; ===================================================
Main:
	mov    ax, cs                ; cs <- 0
    mov    ds, ax                ; ds <- 0
    mov    ss, ax                ; ss <- 0
    mov    ax, BaseOfLoader
    mov    es, ax                ; es <- BaseOfLoader
    mov    sp, BaseOfStack       ; 这几个段寄存器在Main里都不会变了

    mov     ax, .BootMessage
.BootMessage:   db "TestOutput"
    mov     bp, ax
    mov     bx, 000ch
    mov     cx, 10
    mov     dh, 2
    mov     dl, 0
    call    DispStr

    jmp     $

FindLoaderInRootDir:
	mov    ax, [RootDirSectorNow]; ax <- 现在正在搜索的扇区号
    mov    bx, OffsetOfLoader    ; es:bx = BaseOfLoader:OffsetOfLoader  
    mov    cx, 1
    call   ReadSector

    mov    si, LoaderFileName    ; ds:si -> "LOADER  BIN"
    mov    di, OffsetOfLoader    ; es:di -> BaseOfLoader:400h = BaseOfLoader*10h+400h
    mov    dx, 10h               ; 32(目录项大小) * 16(dx) = 512(BPB_BytsPerSec)

CompareFilename:
    call   StringCmp
    cmp    ax, 1
    jz     LoaderFound           ; ax == 1 -> 比对成了
    dec    dx
    cmp    dx, 0                 
    jz     GotoNextRootDirSector ; 该扇区的所有目录项都探索完了，去探索下一个扇区
    add    di, 20h               ; 32 -> 目录项大小
    jmp    CompareFilename

GotoNextRootDirSector:
    inc    word [RootDirSectorNow]      ; 改变正在搜索的扇区号
    dec    word [LeftRootDirSectors]    ; ┓
    cmp    word [LeftRootDirSectors], 0 ; ┣ 判断根目录区是不是已经读完
    jz     NoLoader                     ; ┛ 如果读完表示没有找到 LOADER.BIN，就直接开摆
    jmp    FindLoaderInRootDir

NoLoader:
    mov     ax, .BootMessage
.BootMessage:   db "No Loader"
    mov     bp, ax
    mov     bx, 000ch
    mov     cx, 9
    mov     dh, 1
    mov     dl, 0
    call    DispStr
    jmp     $

LoaderFound:                     ; 找到 LOADER.BIN 后便来到这里继续
    add     di, 01Ah              ; 0x1a = 28 这个 28 在目录项里偏移量对应的数据是起始簇号（RTFM）
    mov     dx, word [es:di]      ; 起始簇号占2字节，读入到dx里
    mov     di, 0               ; 清空保险一下，奇怪的bug
    mov     di, 4
    ; ===
.Disp:
    mov     ax, dx
    call    MyDisp
    shr     dx, 4
    dec     di
    cmp     di, 0
    jnz     .Disp
    ; ===
jmp     $
;     mov     bx, OffsetOfLoader    ; es:bx = BaseOfLoader:OffsetOfLoader  
    

; LoadLoader:
; 	; call   DispDot
;     mov    ax, dx                ; ax <- 数据区簇号
;     add    ax, DeltaSectorNo     ; 数据区的簇号需要加上一个偏移量才能得到真正的扇区号
;     mov    cx, 1                 ; 一个簇就仅有一个扇区
;     ; call   ReadSector
    
;     call   MyDisp

;     mov    ax, dx            
;     call   GetNextCluster        ; 根据数据区簇号获取文件下一个簇的簇号
;     mov    dx, ax                ; dx <- 下一个簇的簇号
;     cmp    dx, 0FFFh             ; 判断是否读完了（根据文档理论上dx只要在0xFF8~0xFFF都行，但是这里直接偷懒只判断0xFFF）
;     jz     LoadFinished
;     add    bx, [BPB_BytsPerSec]  ; 别忘了更新bx，否则你会发现文件发生复写的情况（指来回更新BaseOfLoader:OffsetOfLoader ~ BaseOfLoader:OffsetOfLoader+0x200）
;     jmp    LoadLoader
;     inc    di

; LoadFinished:
;     mov     ax, .BootMessage
; .BootMessage:   db "Ready."
;     mov     bp, ax
;     mov     bx, 000ch
;     mov     cx, 6
;     mov     dh, 1
;     mov     dl, 0
;     call    DispStr
; 	jmp	    $				 ; 到此停住
; ===================================================


