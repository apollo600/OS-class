#include <type.h>
#include <x86.h>

#include <kern/syscall.h>
#include <kern/time.h>
#include <kern/process.h>
#include <kern/trap.h>

static size_t timecounter;

/*
 * 时间戳加一
 */
void
timecounter_inc(void)
{
	timecounter++;
}

/*
 * 获取内核当前的时间戳
 */
size_t
kern_get_ticks(void)
{
	return timecounter;
}

ssize_t
do_get_ticks(void)
{
	return (ssize_t)kern_get_ticks();
}

ssize_t
do_delay_ticks(int pid, u32 ticks, u32 start_tick)
{
	PROCESS* p_proc =  proc_table + pid;
	p_proc->pcb.delay_tick = start_tick + ticks;
	schedule();
	// p_proc->pcb.priority--;
	// if (p_proc->pcb.priority < 0)
	// 	p_proc->pcb.priority = 0;
	return 0;
}