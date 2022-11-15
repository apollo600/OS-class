#ifndef MINIOS_KERN_KMALLOC_H
#define MINIOS_KERN_KMALLOC_H

phyaddr_t	phy_malloc_4k(void);
phyaddr_t	phy_malloc_user(u32 size);
void	 	user_alloc_pte(phyaddr_t cr3, phyaddr_t start_va, u32 size);

#endif /* MINIOS_KERN_KMALLOC_H */