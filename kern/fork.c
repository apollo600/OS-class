#include <assert.h>
#include <x86.h>
#include <string.h>

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
	// 这可能是你第一次实现一个比较完整的功能，你可能会比较畏惧
	// 但是放心，别怕，先别想自己要实现一个这么大的东西而毫无思路
	// 这样你在焦虑的同时也在浪费时间，就跟你在实验五中被页表折磨一样
	// 人在触碰到未知的时候总是害怕的，这是天性，所以请你先冷静下来
	// fork系统调用会一步步引导你写出来，不会让你本科造火箭的
	// panic("Unimplement! CALM DOWN!");
	
	// 推荐是边写边想，而不是想一车然后写，这样非常容易计划赶不上变化

	// fork的第一步你需要找一个空闲（IDLE）的进程作为你要fork的子进程
	// panic("Unimplement! find a idle process");
	PROCESS* proc_fork = get_empty_process();
	if (proc_fork == NULL) {
		kprintf("NO EMPTY PROCESS\n");
		return -1;
	} else {
		memset(proc_fork, 0, sizeof(PROCESS)); // 清空
	}

	// 再之后你需要做的是好好阅读一下pcb的数据结构，搞明白结构体中每个成员的语义
	// 别光扫一遍，要搞明白这个成员到底在哪里被用到了，具体是怎么用的
	// 可能exec和exit系统调用的代码能够帮助你对pcb的理解，不先理解好pcb你fork是无从下手的
	// panic("Unimplement! read pcb");

	// 在阅读完pcb之后终于可以开始fork工作了
	// 本质上相当于将父进程的pcb内容复制到子进程pcb中
	// 但是你需要想清楚，哪些应该复制到子进程，哪些不应该复制，哪些应该子进程自己初始化
	// 其中有三个难点
	// 1. 子进程"fork"的返回值怎么处理？（需要你对系统调用整个过程都掌握比较清楚，如果非常清晰这个问题不会很大）
	// 2. 子进程内存如何复制？（别傻乎乎地复制父进程的cr3，本质上相当于与父进程共享同一块内存，
	// 而共享内存肯定不符合fork的语义，这样一个进程写内存某块地方会影响到另一个进程，这个东西需要你自己思考如何复制父进程的内存）
	// 3. 在fork结束后，肯定会调度到子进程，那么你怎么保证子进程能够正常进入用户态？
	// （你肯定会觉得这个问题问的莫名其妙的，只能说你如果遇到那一块问题了就会体会到这个问题的重要性，
	// 这需要你对调度整个过程都掌握比较清楚）
	// panic("Unimplement! copy pcb?");
	while (xchg(&p_fa->lock, 1) == 1) // 打开父进程的一把大锁
		schedule();
	disable_int();
		memcpy(proc_fork, p_fa, KERN_STACKSIZE); // 先将pcb和内核栈全拷贝过去，然后再进行修改
	enable_int();
	PROCESS_0* p_son = &proc_fork->pcb;
	p_son->statu = INITING;
	/* 分配页表 */
	page_table_copy(p_fa, p_son);
	/* 其他需要初始化的内容 */
	p_son->lock = 0;
	p_son->pid = alloc_pid();
	p_son->priority = p_son->ticks = 1;
	p_son->user_regs.eax = 0;

	// 别忘了维护进程树，将这对父子进程关系添加进去
	// panic("Unimplement! maintain process tree");
	/* 父子节点 */
	p_son->fork_tree.p_fa = p_fa;
	/* 兄弟节点 */
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
	}

	// 最后你需要将子进程的状态置为READY，说明fork已经好了，子进程准备就绪了
	// panic("Unimplement! change status to READY");
	disable_int();
		xchg(&p_son->lock, 0); // 关掉子进程的一把大锁
		xchg(&p_fa->lock, 0); // 关掉父进程的一把大锁
		p_son->statu = READY;
		// kern regs
		p_son->kern_regs.esp = (u32)(proc_fork + 1) - 8;
		// 保证切换内核栈后执行流进入的是restart函数。
		*(u32 *)(p_son->kern_regs.esp + 0) = (u32)restart;
		// 这里是因为restart要用`pop esp`确认esp该往哪里跳。
		*(u32 *)(p_son->kern_regs.esp + 4) = (u32)p_son;
	enable_int();

	// 在你写完fork代码时先别急着运行跑，先要对自己来个灵魂拷问
	// 1. 上锁上了吗？所有临界情况都考虑到了吗？（永远要相信有各种奇奇怪怪的并发问题）
	// 2. 所有错误情况都判断到了吗？错误情况怎么处理？（RTFM->`man 2 fork`）
	// 3. 你写的代码真的符合fork语义吗？
	// panic("Unimplement! soul torture");

	return p_son->pid;
}

ssize_t
do_fork(void)
{
	return kern_fork(&p_proc_ready->pcb);
}