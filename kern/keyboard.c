#include "keymap.h"
#include "stdio.h"
#include "type.h"
#include "trap.h"
#include "x86.h"

#define KB_INBUF_SIZE 4

typedef struct kb_inbuf {
	u8*	p_head;
	u8*	p_tail;
	int	count;
	u8	buf[KB_INBUF_SIZE];
} KB_INPUT;

static KB_INPUT kb_input = {
	.p_head = kb_input.buf,
	.p_tail = kb_input.buf,
	.count = 0,
};

/*
 * 将ch这个字符放进内核的字符缓冲区
 */
void
add_keyboard_buf(u8 ch)
{
	if (kb_input.count < KB_INBUF_SIZE) {
		*(kb_input.p_head) = ch;
		kb_input.p_head++;
		// 如果缓冲区满丢弃其中内容
		if (kb_input.p_head == kb_input.buf + KB_INBUF_SIZE) {
			kb_input.p_head = kb_input.buf;
		}
		kb_input.count++;
	}
}

/*
 * 如果内核的字符缓冲区为空，则返回-1
 * 否则返回缓冲区队头的字符并弹出队头
 */
u8
getch(void)
{
	u8 scan_code;
	int make = 0;
	u8 output;

	if (kb_input.count > 0) {
		disable_int();
		scan_code = *(kb_input.p_tail);
		kb_input.p_tail++;
		if (kb_input.p_tail == kb_input.buf + KB_INBUF_SIZE) {
			kb_input.p_tail = kb_input.buf;
		}
		kb_input.count--;
		enable_int();

		// 解析扫描码
		if (scan_code == 0xE1) {
			/* do nothing */
			return -1;
		} else if (scan_code == 0xE0) {
			/* do nothing */
			return -1;
		} else {
			/* 可打印字符 */
			// 判断是make code还是break code
			make = (scan_code & FLAG_BREAK ? 0 : 1);
			// 如果是make code就打印
			if (make) {
				output = keymap[scan_code & 0x7F];
				return output;
			} else {
				return -1;
			}
		}
	} else {
		return -1;
	}
}