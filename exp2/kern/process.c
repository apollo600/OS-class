#include <elf.h>
#include <mmu.h>
#include <string.h>
#include <x86.h>
#include <assert.h>

#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/process.h>
#include <kern/protect.h>
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

/*
 * 调度函数
 */
void
schedule(void)
{
	// 获取当前的eflags的中断位
	u32 IF_BIT = read_eflags() & FL_IF;
	// 如果中断没关，一定！一定！一定！要关中断保证当前执行流操作的原子性
	if (IF_BIT != 0)
		disable_int();
	
	PROCESS *p_cur_proc = p_proc_ready;
	PROCESS *p_next_proc = p_proc_ready + 1;
	while (p_next_proc != p_cur_proc) {
		// 循环增加
		if (p_next_proc >= proc_table + PCB_SIZE) {
			p_next_proc = proc_table;
		}
		// 如果不在延迟区间
		if (p_next_proc->pcb.delay_tick <= do_get_ticks()) {
			break;
		}
		// 如果在延迟区间，让其他进程先执行
		p_next_proc++;
	}
	if (p_next_proc == p_cur_proc) {
		// 没有可用进程
		// panic("scheduler error");
		// 继续该可用进程不就行了
	}
	

	p_proc_ready = p_next_proc;
	// 切换进程页表和tss
	lcr3(p_proc_ready->pcb.cr3);
	tss.esp0 = (u32)(&p_proc_ready->pcb.user_regs + 1);

	// 调度函数最精髓的部分，非常的有意思。
	// 对于目前的os来说，eax,ebx,ecx,edx,esi,edi,esp,ebp,eflags这九个寄存器就可以表达当前执行流的上下文了，
	// 所以这个函数干了个什么事呢，它先将当前的esp保存到kern_regs中保存当前的栈信息，
	// 之后将esp移到当前kern_context栈顶然后按顺序依次将eax,ecx,edx,ebx,ebp,esi,edi,eflags压栈，
	// 这样就把当前进程寄存器上下文都保存下来了，再之后esp移到目标kern_context栈底，
	// 再依次恢复eflags,edi,esi,ebp,ebx,edx,ecx,eax，
	// 最后一步恢复esp，这个esp本身被push时保留的是进程栈中的esp，这样就完成了执行流的切换。
	// 从当前进程的角度上这就很有意思，相当于执行流进入了一个函数之后突然就被送往其他次元了，而且当前次元的时间静止了。
	// 然后再之后突然又回来了，还是之前的那些寄存器信息，感觉这个函数啥都没干。
	// 由于涉及到一大堆跟执行流切换相关的操作，所以一定！一定！一定！要关中断。
	switch_kern_context(
		&p_cur_proc->pcb.kern_regs, 
		&p_next_proc->pcb.kern_regs);
	// 在传送回之后需要做的第一件事就是忘掉所有的全局变量的缓存
	// 防止编译器优化导致某些变量被优化掉，比如说某个全局变量a
	// 你拿eax去获取a的值，在发生调度之后a的值发生变化，下面用到了a
	// 然后编译器想都不想就直接用eax接着用了，一个bug就这么来了
	
	// 这玩意学术名词叫内存屏障，这还只是单核，如果多核就更加恐怖了，
	// 你会看到内存屏障遍地插，这也是为什么os难的一部分原因，跟缓存斗，跟硬件斗，跟编译器斗，跟并发斗，其乐无穷。
	// 要敬佩那些写系统软件的人，斗都要斗麻了的。
	asm volatile ("":::"memory");
	
	// 这里可能你会比较疑惑为什么还要开中断，
	// 这个时候你需要要以单线程的思路去想，
	// 上面的switch_kern_context在执行后执行流就交给了其他进程了，
	// 但是其他进程总是会通过调度调度回来，对于当前这个进程来说，
	// 它相当于突然被送到其他次元了，包还掉在地上（关中断），而且那个次元的时间就静止了（The World！）
	// 当在其他次元兜兜转转回来了之后，还得把包捡回来（开中断）。
	// 不用去关心其他次元是怎么干的，但是对于这个次元来说还是得捡包的。
	// 如果每个次元都遵守这个规则，就可以保证执行不会出问题。
	if (IF_BIT != 0)
		enable_int();
}

u32
kern_get_pid(PROCESS *p_proc)
{
	return p_proc->pcb.pid;
}

ssize_t
do_get_pid(void)
{
	return (ssize_t)kern_get_pid(p_proc_ready);
}