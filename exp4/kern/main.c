#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "process.h"
#include "protect.h"
#include "type.h"
#include "trap.h"
#include "x86.h"
#include "time.h"

/*
 * 三个测试函数，用户进程的执行流
 */
void TestA()
{
	int i = 0;
	while(1){
		// kprintf("\x1b[31mA%d.\x1b[0m",i++);
		milli_delay(100); //do nothing
	}
}

void TestB()
{
	int i = 0;
	while(1){
		// kprintf("\x1b[31mB%d.\x1b[0m",i++);
		milli_delay(100); //do nothing
	}
}

void TestC()
{
	int i = 0;
	while(1){
		// kprintf("\x1b[31mC%d.\x1b[0m",i++);
		milli_delay(100); //do nothing
	}
}

// 每个栈空间有4kb大
#define STACK_PREPROCESS	0x1000
#define STACK_TOTSIZE		STACK_PREPROCESS * PCB_SIZE
// 用户栈（直接在内核开一个临时数组充当）
char process_stack[STACK_TOTSIZE];
// 指向当前进程pcb的指针
PROCESS *p_proc_ready;
// pcb表
PROCESS	proc_table[PCB_SIZE];
void (*entry[]) = {
	TestA,
	TestB,
	TestC,
};
char pcb_name[PCB_SIZE][16] = {
	"TestA",
	"TestB",
	"TestC",
};
int time_allocate[PCB_SIZE] = {2, 2, 1};

/*
 * 内核的main函数
 * 用于初始化用户进程，然后将执行流交给用户进程
 */
void kernel_main()
{
	kprintf("---start kernel main---\n");

	// 设置8253 PIT
	outb(TIMER_MODE, RATE_GENERATOR);
	outb(TIMER0, (u8) (TIMER_FREQ/HZ) );
	outb(TIMER0, (u8) ((TIMER_FREQ/HZ) >> 8) );
	
	PROCESS *p_proc = proc_table;
	char *p_stack = process_stack;

	for (int i = 0 ; i < PCB_SIZE ; i++, p_proc++) {
		strcpy(p_proc->p_name, pcb_name[i]);
		p_proc->p_left_time = time_allocate[i];
		p_proc->regs.cs = (SELECTOR_FLAT_C & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_USER;
		p_proc->regs.ds = (SELECTOR_FLAT_RW & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_USER;
		p_proc->regs.es = (SELECTOR_FLAT_RW & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_USER;
		p_proc->regs.fs = (SELECTOR_FLAT_RW & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_USER;
		p_proc->regs.ss = (SELECTOR_FLAT_RW & SA_RPL_MASK & SA_TI_MASK)
			| SA_TIL | RPL_USER;
		p_proc->regs.gs = (SELECTOR_VIDEO & SA_RPL_MASK & SA_TI_MASK)
			| RPL_USER;
		
		p_proc->regs.eip = (u32)entry[i];
		p_stack += STACK_PREPROCESS;
		p_proc->regs.esp = (u32)p_stack;
		p_proc->regs.eflags = 0x1202; /* IF=1, IOPL=1 */
	}

	p_proc_ready = proc_table;

	enable_irq(CLOCK_IRQ);
	enable_irq(KEYBOARD_IRQ);

	restart();
	assert(0);
}