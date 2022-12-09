#ifndef MINIOS_KERN_KMALLOC_H
#define MINIOS_KERN_KMALLOC_H

#include <type.h>

void		phy_free_4k(phyaddr_t v);
phyaddr_t	phy_malloc_4k(void);

void		kfree(void *v);
void *		kmalloc(size_t n);

#define PHY_START_ADDR (96 * MB)
#define PHY_END_ADDR (128 * MB)
#define PHY_BLOCK_COUNT ((PHY_END_ADDR - PHY_START_ADDR) / (PGSIZE))

// 初始化队列
void init_km(void);

#endif /* MINIOS_KERN_KMALLOC_H */