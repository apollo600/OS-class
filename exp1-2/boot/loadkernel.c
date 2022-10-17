/*
 * 类型
 */
typedef int		i32;
typedef	unsigned int	u32;
typedef short		i16;
typedef	unsigned short	u16;
typedef char		i8;
typedef	unsigned char	u8;

// 通常描述一个对象的大小，会根据机器的型号变化类型
typedef u32		size_t;
// elf文件格式会用
typedef u32		Elf32_Addr;
typedef u16		Elf32_Half;
typedef u32		Elf32_Off;
typedef i32		Elf32_Sword;
typedef u32		Elf32_Word;

/*
 * elf相关
 */
#define KERNEL_ELF 0x80400

#define EI_NIDENT 16
typedef struct elf32_hdr {
	unsigned char	e_ident[EI_NIDENT];
	Elf32_Half	e_type;
	Elf32_Half	e_machine;
	Elf32_Word	e_version;
	Elf32_Addr	e_entry; /* Entry point */
	Elf32_Off	e_phoff;
	Elf32_Off	e_shoff;
	Elf32_Word	e_flags;
	Elf32_Half	e_ehsize;
	Elf32_Half	e_phentsize;
	Elf32_Half	e_phnum;
	Elf32_Half	e_shentsize;
	Elf32_Half	e_shnum;
	Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6

typedef struct elf32_phdr {
	Elf32_Word	p_type;
	Elf32_Off	p_offset;
	Elf32_Addr	p_vaddr;
	Elf32_Addr	p_paddr;
	Elf32_Word	p_filesz;
	Elf32_Word	p_memsz;
	Elf32_Word	p_flags;
	Elf32_Word	p_align;
} Elf32_Phdr;

/* Legal values for e_machine (architecture).  */

#define EM_NONE		 0	/* No machine */
#define EM_M32		 1	/* AT&T WE 32100 */
#define EM_SPARC	 2	/* SUN SPARC */
#define EM_386		 3	/* Intel 80386 */
#define EM_68K		 4	/* Motorola m68k family */
#define EM_88K		 5	/* Motorola m88k family */
#define EM_IAMCU	 6	/* Intel MCU */
#define EM_860		 7	/* Intel 80860 */
#define EM_MIPS		 8	/* MIPS R3000 big-endian */
#define EM_S370		 9	/* IBM System/370 */
#define EM_MIPS_RS3_LE	10	/* MIPS R3000 little-endian */
/* ... */

/* Legal values for p_flags (segment flags).  */

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */

/*
 * 显示相关
 */
#define TERMINAL_COLUMN	80
#define TERMINAL_ROW	25

#define TERMINAL_POS(row, column) ((u16)(row) * TERMINAL_COLUMN + (column))

#define NUM_TO_ASCII 48
#define CHAR_TO_ASCII 55
/*
 * 终端默认色，黑底白字
 */
#define DEFAULT_COLOR 0x0f00

/*
 * 这个函数将content数据（2字节）写到终端第disp_pos个字符
 * 第0个字符在0行0列，第1个字符在0行1列，第80个字符在1行0列，以此类推
 * 用的是内联汇编，等价于mov word [gs:disp_pos * 2], content
 */
inline static void 
write_to_terminal(u16 disp_pos, u16 content)
{
	asm(
	    "mov %1, %%gs:(%0)" ::"r"(disp_pos * 2), "r"(content)
	    : "memory");
}

/*
 * 清屏
 */
static void
clear_screen()
{
	for (int i = 0; i < TERMINAL_ROW; i++)
		for (int j = 0; j < TERMINAL_COLUMN; j++)
			write_to_terminal(TERMINAL_POS(i, j), 
							DEFAULT_COLOR | ' ');
}

static void *
memset(void *v, int c, size_t n)
{
	char *p;
	int m;

	p = v;
	m = n;
	while (--m >= 0)
		*p++ = c;

	return v;
}

static void *
memcpy(void *dst, const void *src, size_t n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;

	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0)
			*--d = *--s;
	} else {
		while (n-- > 0)
			*d++ = *s++;
	}

	return dst;
}

u16 
num_to_char(u32 num, u8 *res, u8 radix) {
	u8 tmp[10];
	u16 count = 0;
	while (num != 0) {
		tmp[count] = num % radix;
		tmp[count] += tmp[count] < 10 ? NUM_TO_ASCII : CHAR_TO_ASCII; 
		count++;
		num /= radix;
	}
	for (u16 i = 0; i < count; i++)
		res[i] = tmp[count-1-i];
	res[count] = '\0';
	return count;
}

/*
 * 初始化函数，加载kernel.bin的elf文件并跳过去。
 */
void 
load_kernel()
{
	u32 global_line = 0;
	u32 line_index = 0;
	
	clear_screen();
	for (char *s = "----start loading kernel elf----", *st = s; *s; s++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);

	Elf32_Ehdr *kernel_ehdr = (Elf32_Ehdr *)KERNEL_ELF;
	Elf32_Phdr *kernel_phdr = (void *)kernel_ehdr + kernel_ehdr->e_phoff;

	// 读取'ELF'
	global_line = 2;
	line_index = 0;
	for (char *s = "File type: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	for (u8 i = 1; i < 4; i++)
		write_to_terminal(i - 1 + line_index + global_line*80, DEFAULT_COLOR | (u8)kernel_ehdr->e_ident[i]);

	// 读取'i386'
	global_line = 3;
	line_index = 0;
	if (kernel_ehdr->e_machine == EM_386)
		for (char *s = "Machine type: Intel 80386", *st = s; *s; s++, line_index++)
			write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	
	// 输出程序段数量、在内存中开始的地址、加载的长度、每个段的读/写/可执行标志
	global_line = 4;
	line_index = 0;
	for (char *s = "Program session: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	
	// 数量
	global_line = 5;
	line_index = 0;
	for (char *s = "-- phnum: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	u16 phnum = kernel_ehdr->e_phnum;
	u8 phnum_char[10];
	num_to_char(phnum, phnum_char, 10);
	for (u8 *s = phnum_char, *st = s; *s; s++)
		write_to_terminal(s - st + global_line*80 + line_index, DEFAULT_COLOR | *s);
	
	// 开始的地址
	global_line = 6;
	line_index = 0;
	for (char *s = "-- paddr: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	Elf32_Off paddr = (Elf32_Off)((void *)kernel_ehdr + kernel_phdr->p_offset);
	u8 paddr_char[10];
	num_to_char(paddr, paddr_char, 16);
	for (u8 *s = paddr_char, *st = s; *s; s++)
		write_to_terminal(s - st + global_line*80 + line_index, DEFAULT_COLOR | *s);
	
	// 加载的长度(filesize)
	global_line = 7;
	line_index = 0;
	for (char *s = "-- pfilesz: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	Elf32_Word pfilesz = 0;
	Elf32_Word pmemsz = 0;
	for (u32 i = 0; i < kernel_ehdr->e_phnum; i++, kernel_phdr++) {
		pfilesz += kernel_phdr->p_filesz;
		pmemsz += kernel_phdr->p_memsz;
	}
	u8 pfilesz_char[10];
	num_to_char(pfilesz, pfilesz_char, 10);
	for (u8 *s = pfilesz_char, *st = s; *s; s++)
		write_to_terminal(s - st + global_line*80 + line_index, DEFAULT_COLOR | *s);
	
	// 加载的长度(memsize)
	global_line = 8;
	line_index = 0;
	for (char *s = "-- pmemsz: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	u8 pmemsz_char[10];
	num_to_char(pmemsz, pmemsz_char, 10);
	for (u8 *s = pmemsz_char, *st = s; *s; s++)
		write_to_terminal(s - st + global_line*80 + line_index, DEFAULT_COLOR | *s);
	
	// 每个段的权限设置
	global_line = 9;
	line_index = 0;
	for (char *s = "-- pflags: ", *st = s; *s; s++, line_index++)
		write_to_terminal(s - st + global_line*80, DEFAULT_COLOR | *s);
	kernel_phdr = (void *)kernel_ehdr + kernel_ehdr->e_phoff; // 复原
	for (u32 i = 0; i < kernel_ehdr->e_phnum; i++, kernel_phdr++) {
		char t_content = kernel_phdr->p_flags == PF_X ? 'X' : kernel_phdr->p_flags == PF_R ? 'W' : 'R';
		write_to_terminal(global_line*80 + line_index, DEFAULT_COLOR | t_content);
		line_index++;
		write_to_terminal(global_line*80 + line_index, DEFAULT_COLOR | ' ');
		line_index++;
	}

	// 执行kernel程序
	kernel_phdr = (void *)kernel_ehdr + kernel_ehdr->e_phoff; // 复原
	for (u32 i = 0; i < kernel_ehdr->e_phnum; i++, kernel_phdr++)
	{
		if (kernel_phdr->p_type != PT_LOAD)
			continue;
		// 将elf的文件数据复制到指定位置
		memcpy(
		    (void *)kernel_phdr->p_vaddr,
		    (void *)kernel_ehdr + kernel_phdr->p_offset,
		    kernel_phdr->p_filesz);
		// 将后面的字节清零(p_memsz >= p_filesz)
		memset(
		    (void *)kernel_phdr->p_vaddr + kernel_phdr->p_filesz,
		    0,
		    kernel_phdr->p_memsz - kernel_phdr->p_filesz);
	}
	((void (*)(void))(kernel_ehdr->e_entry))();
}