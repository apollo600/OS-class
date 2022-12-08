#include <x86.h>
#include <assert.h>

#include <kern/process.h>
#include <kern/sche.h>
#include <kern/syscall.h>
#include <kern/stdio.h>

/*
 * 这个函数是写给汇编函数用，
 * 在汇编函数调用save函数保存完用户寄存器上下文后，
 * 需要切到进程内核栈中，minios的进程栈是与pcb是绑定的。
 * 相当于每个进程分配了一个大空间，前面一小部分给了pcb，
 * 后面的全可以给进程栈使用，目前进程占8KB，进程栈占7KB多。
 * 
 * 这个函数的目的就是切换到进程栈处，由于每个进程的空间只有C语言这知道，
 * 所以直接在C语言里面写一个内联汇编实现，这样更方便动态开内核栈空间不用顾忌到汇编。
 */
void
to_kern_stack(u32 ret_esp)
{
	asm volatile (
		"add %1, %0\n\t"
		"mov %0, %%esp\n\t"
		"push %2\n\t"
		"jmp *4(%%ebp)\n\t":
		: "a"((u32)p_proc_ready->_pad)
		, "b"((u32)sizeof(p_proc_ready->_pad))
		, "c"(ret_esp)
	);
}

u32
kern_get_pid(PROCESS_0 *p_proc)
{
	u32 ret = 0;
	while (xchg(&p_proc->lock, 1) == 1)
		schedule();
	ret = p_proc->pid;
free:
	xchg(&p_proc->lock, 0);
	return ret;
}

ssize_t
do_get_pid(void)
{
	return (ssize_t)kern_get_pid(&p_proc_ready->pcb);
}

PROCESS*
get_empty_process(void) {
	PROCESS* proc_current = proc_table + 1;
	// kprintf("%d | ", proc_table->pcb.ticks);
	while (proc_current != proc_table) {
		// 加锁取数据
		// kprintf("%d | ", proc_current->pcb.ticks);
		while (xchg(&proc_current->pcb.lock, 1) == 1)
			schedule();
		if (proc_current->pcb.statu == IDLE) {
			return proc_current;
		}
		xchg(&proc_current->pcb.lock, 0);
		proc_current++;
		if (proc_current - proc_table >= PCB_SIZE) {
			proc_current = proc_table;
		}
	}
	return NULL;
}

void
init_pid(void) {
	u32 i;
	for (i = 0; i < PID_COUNT - 1; i++) {
		pid_map[i] = i + 1;
	}
	pid_map[PID_COUNT - 1] = 1;
}

u32
alloc_pid(void) {
	u32 new_pid = pid_map[0]; // 取可用pid
	if (new_pid == 0) {
		panic("pid run out");
	}
	assert(new_pid < PID_COUNT); // 防止越界
	pid_map[0] = pid_map[new_pid]; // 更新下一个可用的pid
	return new_pid;
}

void
free_pid(u32 old_pid) {
	u32 i;
	u32 old_index = 0;
	u32 new_index = pid_map[0];
	assert(old_pid > 0 && old_pid <= PID_COUNT);
	// 遍历map找到该pid的位置
	for (i = 1; i < PID_COUNT; i++) {
		if (pid_map[i] == old_pid) {
			old_index = i;
			break;
		}
	}
	// 跳过PID_DELAY-1个pid
	for (i = 0; i < PID_DELAY - 1; i++) {
		assert(new_index < PID_COUNT);
		new_index = pid_map[new_index];
	}
	// 插入到tmp之后
	assert(new_index < PID_COUNT);
	assert(old_index < PID_COUNT);
	u32 tmp = pid_map[new_index];
	pid_map[new_index] = old_index; // 这样原来的旧pid会继续被分配出去
	pid_map[old_index] = tmp;
	pid_map[PID_COUNT - 1] = old_pid; // 维护pid总数不变
}
