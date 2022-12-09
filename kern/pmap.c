#include <assert.h>
#include <elf.h>
#include <string.h>
#include <mmu.h>
#include <x86.h>

#include <kern/kmalloc.h>
#include <kern/pmap.h>
#include <kern/stdio.h>
#include <kern/trap.h>

/*
 * 申请一个新的物理页，并更新page_list页面信息
 * 返回新申请的物理页面的物理地址
 */
static phyaddr_t
alloc_phy_page(struct page_node **page_list)
{
	phyaddr_t paddr = phy_malloc_4k();
	
	struct page_node *new_node = kmalloc(sizeof(struct page_node));
	new_node->nxt = *page_list;
	new_node->paddr = paddr;
	new_node->laddr = -1;
	*page_list = new_node;

	return paddr;
}

/*
 * MINIOS中比较通用的页表映射函数
 * 它将laddr处的虚拟页面映射到物理地址为paddr（如果paddr为-1则会自动申请一个新的物理页面）的物理页面
 * 并将pte_flag置位到页表项（页目录项标志位默认为PTE_P | PTE_W | PTE_U）
 * 这个函数中所有新申请到的页面信息会存放到page_list这个链表中
 */
static void
lin_mapping_phy(u32			cr3,
		struct page_node	**page_list,
		uintptr_t		laddr,
		phyaddr_t		paddr,
		u32			pte_flag)
{
	assert(PGOFF(laddr) == 0);

	uintptr_t *pde_ptr = (uintptr_t *)K_PHY2LIN(cr3);

	if ((pde_ptr[PDX(laddr)] & PTE_P) == 0) {
		phyaddr_t pte_phy = alloc_phy_page(page_list);
		memset((void *)K_PHY2LIN(pte_phy), 0, PGSIZE);
		pde_ptr[PDX(laddr)] = pte_phy | PTE_P | PTE_W | PTE_U;
	}

	phyaddr_t pte_phy = PTE_ADDR(pde_ptr[PDX(laddr)]);
	uintptr_t *pte_ptr = (uintptr_t *)K_PHY2LIN(pte_phy);

	phyaddr_t page_phy;
	if (paddr == (phyaddr_t)-1) {
		if ((pte_ptr[PTX(laddr)] & PTE_P) != 0)
			return;
		page_phy = alloc_phy_page(page_list);
		(*page_list)->laddr = laddr;
	} else {
		if ((pte_ptr[PTX(laddr)] & PTE_P) != 0)
			warn("this page was mapped before, laddr: %x", laddr);
		assert(PGOFF(paddr) == 0);
		page_phy = paddr;
		struct page_node *new_node = kmalloc(sizeof(struct page_node));
		new_node->nxt = *page_list;
		new_node->paddr = paddr;
		new_node->laddr = laddr;
		*page_list = new_node;
	}
	pte_ptr[PTX(laddr)] = page_phy | pte_flag;
}

/*
 * 初始化进程页表的内核部分
 * 将3GB ~ 3GB + 128MB的线性地址映射到0 ~ 128MB的物理地址
 */
void
map_kern(u32 cr3, struct page_node **page_list)
{
	for (phyaddr_t paddr = 0 ; paddr < 128 * MB ; paddr += PGSIZE) {
		lin_mapping_phy(cr3,
				page_list,
				K_PHY2LIN(paddr),
				paddr,
				PTE_P | PTE_W | PTE_U);
	}
}

/*
 * 根据elf文件信息将数据搬迁到指定位置
 * 中间包含了对页表的映射，eip的置位
 * 这也是你们做实验五中最折磨的代码，可以看这份学习一下
 */
void
map_elf(PROCESS_0 *p_proc, void *elf_addr)
{
	assert(p_proc->lock != 0);

	struct Elf *eh = (struct Elf *)elf_addr;

	struct Proghdr *ph = (struct Proghdr *)(elf_addr + eh->e_phoff);
	for (int i = 0 ; i < eh->e_phnum ; i++, ph++) {
		if (ph->p_type != PT_LOAD)
			continue;
		uintptr_t st = ROUNDDOWN(ph->p_va, PGSIZE);
		uintptr_t en = ROUNDUP(st + ph->p_memsz, PGSIZE);
		for (uintptr_t laddr = st ; laddr < en ; laddr += PGSIZE) {
			u32 pte_flag = PTE_P | PTE_U;
			if ((ph->p_flags & ELF_PROG_FLAG_WRITE) != 0)
				pte_flag |= PTE_W;
			lin_mapping_phy(p_proc->cr3,
					&p_proc->page_list,
					laddr,
					(phyaddr_t)-1,
					pte_flag);
		}
		memcpy(	(void *)ph->p_va,
			(const void *)eh + ph->p_offset,
			ph->p_filesz);
		memset(	(void *)ph->p_va + ph->p_filesz, 
			0, 
			ph->p_memsz - ph->p_filesz);
	}

	p_proc->user_regs.eip = eh->e_entry;
}

/*
 * 将用户栈映射到用户能够访问到的最后一个页面
 * (0xbffff000~0xc0000000)
 * 并将esp寄存器放置好
 */
void
map_stack(PROCESS_0 *p_proc)
{
	assert(p_proc->lock != 0);

	lin_mapping_phy(p_proc->cr3,
			&p_proc->page_list,
			K_PHY2LIN(-PGSIZE),
			(phyaddr_t)-1,
			PTE_P | PTE_W | PTE_U);
	
	p_proc->user_regs.esp = K_PHY2LIN(0);
}

/*
 * 根据page_list回收所有的页面（包括回收页面节点）
 */
void
recycle_pages(struct page_node *page_list)
{
	for (struct page_node *prevp, *p = page_list ; p ;) {
		phy_free_4k(p->paddr);
		prevp = p, p = p->nxt;
		kfree(prevp);
	}
}

/**
 * 用于在拷贝进程时拷贝页表
 * 通过遍历page_list链表的方式实现
 * 对于3GB~3GB+128MB部分，直接拷贝对应的物理地址
 * 对于3GB以下部分，先重新分配一个物理地址，再将内容拷贝过去
 */
void
page_table_copy(PROCESS_0* src_pcb, PROCESS_0* dst_pcb) {
	/* 页表创建 */
	phyaddr_t new_cr3 = phy_malloc_4k();
	memset((void *)K_PHY2LIN(new_cr3), 0, PGSIZE);
	struct page_node *new_page_list = kmalloc(sizeof(struct page_node));
	new_page_list->nxt = NULL;
	new_page_list->paddr = new_cr3;
	new_page_list->laddr = -1;
	map_kern(new_cr3, &new_page_list);

	phyaddr_t src_cr3 = src_pcb->cr3;
	phyaddr_t dst_cr3 = new_cr3;
	struct page_node *src_page_list = src_pcb->page_list;
	struct page_node *dst_page_list = new_page_list;
	// kprintf("pcb: %x %x\n", src_pcb, dst_pcb);
	// kprintf("cr3: %x %x\n", src_cr3, dst_cr3);
	// kprintf("page_list: %x %x\n", src_page_list, dst_page_list);
	uintptr_t *buf_page = (uintptr_t *)kmalloc(PGSIZE);
	struct page_node *p = src_page_list;
	// 以下遍历每一个存在的页表项拷贝其内容
	// kprintf("pmap> ");
	while (p != NULL) {
		// 如果是-1，可以直接跳过
		if (p->laddr == -1) {
			p = p->nxt;
			continue;
		}
		// 如果是3GB~3GB+128MB，已经使用map_kern分配过了
		if (p->laddr >= 3 * GB) {
			p = p->nxt;
			continue;
		}
		// 如果是小于3GB，先分配物理地址，然后拷贝数据内容
		else {
			lin_mapping_phy(
				dst_cr3,
				&dst_page_list,
				p->laddr,
				(phyaddr_t)-1,
				PTE_P | PTE_W | PTE_U
			);
			// kprintf("%x: %x=>%x, ", p->laddr, p->paddr, dst_page_list->paddr);
			// 使用内核物理地址的拷贝方式
			// memcpy((uintptr_t *)K_PHY2LIN(dst_page_list->paddr), (uintptr_t *)K_PHY2LIN(p->paddr), PGSIZE);
			// 使用lcr3的方式（正统方式）
			memcpy(buf_page, (void *)p->laddr, PGSIZE);
			DISABLE_INT();
				lcr3(dst_cr3);
				memcpy((void *)p->laddr, buf_page, PGSIZE);
				lcr3(src_cr3);
			ENABLE_INT();
		}
		p = p->nxt;
	}
	kfree(buf_page);
	DISABLE_INT();
		dst_pcb->cr3 = dst_cr3;
		dst_pcb->page_list = dst_page_list;
	ENABLE_INT();
	// kprintf("\n");
}

