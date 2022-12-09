#include <assert.h>
#include <x86.h>
#include <string.h>
#include <errno.h>

#include <kern/fork.h>
#include <kern/syscall.h>
#include <kern/process.h>
#include <kern/stdio.h>
#include <kern/kmalloc.h>
#include <kern/pmap.h>
#include <kern/sche.h>
#include <kern/trap.h>

extern PROCESS *p_proc_ready;

ssize_t
kern_fork(PROCESS_0 *p_fa)
{
	/* Step1: 从进程表找一个空闲进程，如果找不到则报错-EAGAIN */
	PROCESS* proc_fork = get_empty_process();
	if (proc_fork == NULL) {
		return -EAGAIN;
	} else {
		memset(proc_fork, 0, sizeof(PROCESS)); // 清空
	}

	/* Step2: 拷贝父进程的pcb，页表到子进程 */
	PROCESS_0* p_son = &proc_fork->pcb;
	DISABLE_INT();
		p_son->kern_regs = p_fa->kern_regs;
		p_son->user_regs = p_fa->user_regs;
		p_son->lock = 0;
		p_son->statu = INITING;
	ENABLE_INT();
	// 分配页表
	page_table_copy(p_fa, p_son);
	// 其他需要初始化的内容
	p_son->exit_code = 0;
	p_son->pid = alloc_pid();
	kprintf("%d fork %d, ", p_fa->pid, p_son->pid);
	p_son->priority = p_fa->priority;
	p_son->ticks = 1; // 如果给为1,testwait的sleep会出错；如果给为0,会一直抢占时间片
	p_son->user_regs.eax = 0;

	/* Step3: 维护进程树 */
	while (xchg(&p_fa->lock, 1) == 1)
		schedule();

	// 父子节点
	p_son->fork_tree.p_fa = p_fa;
	// 兄弟节点
	if (p_fa->fork_tree.sons == NULL) {
		// 如果之前没有子节点
		struct son_node *new_son_node = (struct son_node *)kmalloc(sizeof(struct son_node));
		new_son_node->nxt = NULL;
		new_son_node->pre = NULL;
		new_son_node->p_son = p_son;
		p_fa->fork_tree.sons = new_son_node;
	} else {
		// 如果已经有子节点
		struct son_node *new_son_node = (struct son_node *)kmalloc(sizeof(struct son_node));
		new_son_node->p_son = p_son;
		new_son_node->pre = p_fa->fork_tree.sons->pre; // 头插法
		new_son_node->nxt = p_fa->fork_tree.sons; 
		p_fa->fork_tree.sons->pre = new_son_node;
		p_fa->fork_tree.sons = new_son_node;
	}
	assert(p_fa->fork_tree.sons->pre == NULL);
	xchg(&p_fa->lock, 0);
	// 清空被fork进程的sons  debug了好久...
	p_son->fork_tree.sons = NULL;

	/* Step4: 将子进程的状态设为READY，开始运行子进程 */
	DISABLE_INT();
		p_son->statu = READY;
		// kern regs
		p_son->kern_regs.esp = (u32)(proc_fork + 1) - 8;
		// 保证切换内核栈后执行流进入的是restart函数。
		*(u32 *)(p_son->kern_regs.esp + 0) = (u32)restart;
		// 这里是因为restart要用`pop esp`确认esp该往哪里跳。
		*(u32 *)(p_son->kern_regs.esp + 4) = (u32)p_son;
	ENABLE_INT();

	/* Step5: 灵魂拷问 */
	// 1. 上锁上了吗？所有临界情况都考虑到了吗？（永远要相信有各种奇奇怪怪的并发问题）
	// 2. 所有错误情况都判断到了吗？错误情况怎么处理？（RTFM->`man 2 fork`）
	// 3. 你写的代码真的符合fork语义吗？

	return p_son->pid;
}

ssize_t
do_fork(void)
{
	return kern_fork(&p_proc_ready->pcb);
}