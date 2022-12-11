#include <assert.h>
#include <mmu.h>
#include <string.h>
#include <errno.h>
#include <x86.h>

#include <kern/syscall.h>
#include <kern/wait.h>
#include <kern/stdio.h>
#include <kern/process.h>
#include <kern/sche.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/kmalloc.h>

ssize_t
kern_wait(int *wstatus)
{
	// 相比于fork来说，wait的实现简单很多
	// 语义实现比较清晰，没有fork那么多难点要处理，所以这里并不会给大家太多引导
	// 需要大家自己思考wait怎么实现。

	// 在实现之前你必须得读一遍文档`man 2 wait`
	// 了解到wait大概要做什么
	// panic("Unimplement! Read The F**king Manual");

	// 当然读文档是一方面，最重要的还是代码实现
	// wait系统调用与exit系统调用关系密切，所以在实现wait之前需要先读一遍exit为好
	// 可能读完exit的代码你可能知道wait该具体做什么了
	// panic("Unimplement! Read The F**king Source Code");

	// 接下来就是你自己的实现了，我们在设计的时候这段代码不会有太大问题
	// 在实现完后你任然要对自己来个灵魂拷问
	// 1. 上锁上了吗？所有临界情况都考虑到了吗？（永远要相信有各种奇奇怪怪的并发问题）
	// 2. 所有错误情况都判断到了吗？错误情况怎么处理？（RTFM->`man 2 wait`）
	// 3. 是否所有的资源都正确回收了？
	// 4. 你写的代码真的符合wait语义吗？

	/* Step1: 判断父进程是否有子进程 */
	while (xchg(&p_proc_ready->pcb.lock, 1) == 1)
		schedule();
	// // 如果没有子进程
	if (p_proc_ready->pcb.fork_tree.sons == NULL) {
		xchg(&p_proc_ready->pcb.lock, 0);
		return -ECHILD;
	}

	/* Step2: 遍历当前进程的所有子进程，找僵尸进程 */
	PROCESS_0* p_fa = &p_proc_ready->pcb;
	struct son_node* p = p_fa->fork_tree.sons;
	
	while (true) {
		// 检测父进程是否正常运行
		if ((p_fa->statu != READY) && (p_fa->statu != SLEEP)) {
			xchg(&p_proc_ready->pcb.lock, 0);
			return -EEXIST;
		}
		// 遍历了一次没找到
		if (p == NULL) {
			p = p_fa->fork_tree.sons;
			xchg(&p_proc_ready->pcb.lock, 0); // 执行别的进程，可以暂时把锁放开
			schedule();
			while (xchg(&p_proc_ready->pcb.lock, 1) == 1)
				schedule(); // 把锁上回来
		}
		// 回收该进程
		if (p->p_son->statu == ZOMBIE) {
			// kprintf("%d kill %d, ", p_fa->pid, p->p_son->pid);
			break;
		}
		// 下一个进程
		p = p->nxt;
	}

	/* Step3: 回收僵尸进程 */
	PROCESS_0* p_zombie = p->p_son;
	// 修改成了一起加锁的模式，减少死锁的概率
// 	while (1) {
// 		if (xchg(&p_zombie->lock, 1) == 1)
// 			goto loop;
// 		if (xchg(&p_fa->lock, 1) == 1)
// 			goto free;
// 		break;
// free:
// 		xchg(&p_zombie->lock, 0);
// loop:
// 		schedule();
// 	}
	u32 ret = p_zombie->pid;
	// 更新进程树
	assert(p_fa->fork_tree.sons->pre == NULL);
	if (p->pre == NULL) {
		// 如果只有它一个子节点
		if (p->nxt == NULL) {
			p_fa->fork_tree.sons = NULL;
		} 
		// 如果是第一个子节点，且后面还有其他节点
		else {
			p->nxt->pre = NULL;
			p_fa->fork_tree.sons = p->nxt;
			assert(p_fa->fork_tree.sons->pre == NULL);
		}
	}
	else 
	{
		if (p->nxt == NULL) {
			// 如果是最后一个子节点，且前面还有其他节点
			p->pre->nxt = NULL;
		} 
		else {
			// 如果前面和后面都有其他节点
			p->pre->nxt = p->nxt;
			p->nxt->pre = p->pre;
		}
	}
	if (p_fa->fork_tree.sons != NULL)
		assert(p_fa->fork_tree.sons->pre == NULL);
	// 返回状态值
	if (wstatus != NULL)
		*wstatus = p_zombie->exit_code;
	// 进行释放
	DISABLE_INT();
		// 回收分配的物理地址
		lcr3(p_zombie->cr3);
		recycle_pages(p_zombie->page_list);
		lcr3(p_fa->cr3);
		p_zombie->cr3 = 0;
	ENABLE_INT();
	// 其他值设置
	p_zombie->exit_code = 0;
	p_zombie->fork_tree.p_fa = NULL;
	p_zombie->fork_tree.sons = NULL;
	p_zombie->priority = 0;
	p_zombie->ticks = 0;
	// 回收pid
	free_pid(p_zombie->pid);
	p_zombie->pid = 0;
	// 最后设置进程状态
	p_zombie->statu = IDLE;
	// p_fa->statu = READY;

	kfree(p);
	xchg(&p_fa->lock, 0);
	// xchg(&p_zombie->lock, 0);
	return ret;
}

ssize_t
do_wait(int *wstatus)
{
	assert((uintptr_t)wstatus < KERNBASE);
	assert((uintptr_t)wstatus + sizeof(wstatus) < KERNBASE);
	return kern_wait(wstatus);
}