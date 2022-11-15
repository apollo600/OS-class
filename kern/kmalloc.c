#include <assert.h>
#include <mmu.h>

#include <kern/kmalloc.h>
#include <kern/trap.h>
#include <kern/stdio.h>

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

static phyaddr_t malloc_4k_p = 64 * MB;

/*
 * 分配物理内存，每次分配4kb，一页
 * 分配的物理内存区间为64MB~128MB
 */
phyaddr_t
phy_malloc_4k(void)
{
	assert(malloc_4k_p < 128 * MB);

	phyaddr_t addr = malloc_4k_p;
	malloc_4k_p += PGSIZE;
	
	return addr;
}

// 在PTE中分配一项给user
void user_alloc_pte(phyaddr_t cr3, phyaddr_t start_va, u32 size) {
	assert(PGOFF(size) == 0);

	// 向下4K对齐
	start_va = (start_va & 0xfffff000);

	u32 pde_num = size / (4 * MB) + (size % (4 * MB) != 0);
	kprintf("malloc: start_va %x, size %d(4K), pde_num %d\n", start_va, size / (4 * KB), pde_num);

	// 获取页表目录项PDE
	phyaddr_t* pde_ptr = (phyaddr_t*)K_PHY2LIN(cr3);
	pde_ptr += PDX(start_va);

	// 只有第一次需要根据偏移量获取首地址
	u32 isfirst = 1;
	uintptr_t *pte_ptr;
	u32 size_to_malloc = size;

	// 不检测是否已分配页表
	// 给目录项分配页表
	while (pde_num--) {
		phyaddr_t phy_pte = phy_malloc_4k();
		assert(PGOFF(phy_pte) == 0);
		*pde_ptr++ = phy_pte | PTE_P | PTE_W | PTE_U;
		
		// 获取页表项起始项
		u32 pte_num;
		if (isfirst == 1) {
			pte_ptr = (uintptr_t*)K_PHY2LIN(phy_pte) + PTX(start_va);
			// 此次分配的页表项个数
			if (size_to_malloc <= ((0x3ff - PTX(start_va) + 1) * 4 * KB)) {
				pte_num = size_to_malloc / (4 * KB);
			} else {
				pte_num = 0x3ff - PTX(start_va) + 1;
			}	
			isfirst = 0;
		} else {
			pte_ptr = (uintptr_t*)K_PHY2LIN(phy_pte);
			// 此次分配的页表项个数
			if (size_to_malloc <= 4 * KB) {
				pte_num = size_to_malloc / (4 * KB);
			} else {
				pte_num = NPTENTRIES;
			}
		}
		// 分配页表
		// kprintf("ptx: %d pte_ptr: %x size_to_malloc: %d(4K) pte_num: %d\n", PTX(start_va), pte_ptr, size_to_malloc / (4 * KB), pte_num);
		while (pte_num--) {
			phyaddr_t pa = phy_malloc_4k();
			*pte_ptr++ = pa | PTE_P | PTE_W | PTE_U; 
			size_to_malloc -= PGSIZE;
		}
	}
	assert(size_to_malloc == 0);
}