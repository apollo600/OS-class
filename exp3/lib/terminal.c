#include "terminal.h"
#include "cmatrix.h"
#include <stdarg.h>

inline static void 
write_to_terminal(u16 disp_pos, u16 content)
{
	asm(
	    "mov %1, %%gs:(%0)" ::"r"(disp_pos * 2), "r"(content)
	    : "memory");
}
void 
clear_screen()
{
	u16 content = DEFAULT_COLOR | ' ';
	for (int i = 0; i < TERMINAL_ROW; i++)
		for (int j = 0; j < TERMINAL_COLUMN; j++)
			write_to_terminal(TERMINAL_POS(i, j), content);
}

void kprintf(u16 disp_pos, const char *format, ...)
{
	// 可变参数数组
	va_list p_args;
	// 绑定可变参数
	va_start(p_args, format);

	u16 i = 0;
	u16 g_content;
	u16 g_background = BACKGROUND(BLACK);
	u16 g_foreground = FOREGROUND(WHITE);
	u16 g_pos = disp_pos;

	while (format[i] != '\0') {
		if (format[i] == '%') {
			i++;
			switch (format[i]) {
				case 'c':
					g_content = (u16)va_arg(p_args, int);
					write_to_terminal(g_pos, g_background | g_foreground | g_content);
					g_pos++;
					break;
				case 'b':
					u16 new_background = (u16)va_arg(p_args, int);
					g_background = BACKGROUND(new_background);
					break;
				case 'f':
					u16 new_foreground = (u16)va_arg(p_args, int);
					g_foreground = FOREGROUND(new_foreground);
					break;
				case 's':
					break;
			}
		}
		else {
			g_content = format[i];
			write_to_terminal(g_pos, g_background | g_foreground | g_content);
			g_pos++;
		}

		i++;
	}

	// 释放可变参数
	va_end(p_args);
}