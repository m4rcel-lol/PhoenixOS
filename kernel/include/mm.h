#ifndef KERNEL_MM_H
#define KERNEL_MM_H

#include "kernel.h"

/* ── Address conversion ───────────────────────────────────────────────────── */

#define PHYS_TO_VIRT(p)  ((virt_addr_t)(p) + KERNEL_BASE)
#define VIRT_TO_PHYS(v)  ((phys_addr_t)(v) - KERNEL_BASE)

/* ── Page flags ───────────────────────────────────────────────────────────── */

#define PF_PRESENT   (1UL << 0)
#define PF_WRITE     (1UL << 1)
#define PF_USER      (1UL << 2)
#define PF_PWT       (1UL << 3)
#define PF_PCD       (1UL << 4)
#define PF_ACCESSED  (1UL << 5)
#define PF_DIRTY     (1UL << 6)
#define PF_HUGE      (1UL << 7)
#define PF_NX        (1UL << 63)

/* ── VM region flags ──────────────────────────────────────────────────────── */

#define VM_READ      (1 << 0)
#define VM_WRITE     (1 << 1)
#define VM_EXEC      (1 << 2)
#define VM_USER      (1 << 3)
#define VM_SHARED    (1 << 4)
#define VM_ANON      (1 << 5)
#define VM_STACK     (1 << 6)

/* ── mmap flags (SYS_MMAP) ────────────────────────────────────────────────── */

#define MMAP_PROT_NONE   0x00
#define MMAP_PROT_READ   0x01
#define MMAP_PROT_WRITE  0x02
#define MMAP_PROT_EXEC   0x04

#define MMAP_MAP_PRIVATE  0x02
#define MMAP_MAP_SHARED   0x01
#define MMAP_MAP_ANON     0x20
#define MMAP_MAP_FIXED    0x10

/* ── Physical page descriptor ─────────────────────────────────────────────── */

struct page {
    u32  ref_count;
    u32  flags;
    struct page *next;          /* free list link */
};

/* ── PMM interface ────────────────────────────────────────────────────────── */

struct multiboot_mmap_entry {
    u64 base_addr;
    u64 length;
    u32 type;                   /* 1 = available */
    u32 reserved;
} __packed;

void       pmm_init(struct multiboot_mmap_entry *entries, u32 count);
phys_addr_t alloc_page(void);
void        free_page(phys_addr_t addr);
u64         pmm_get_free_pages(void);
u64         pmm_get_total_pages(void);
void        pmm_print_stats(void);

/* ── VMM interface ────────────────────────────────────────────────────────── */

void  vmm_init(void);
int   map_page(phys_addr_t phys, virt_addr_t virt, u32 flags);
void  unmap_page(virt_addr_t virt);
u64  *get_pml4(void);

/* ── Kernel heap ──────────────────────────────────────────────────────────── */

void *kmalloc(usize size);
void *kzmalloc(usize size);
void *krealloc(void *ptr, usize new_size);
void  kfree(void *ptr);

/* ── VM area descriptor ───────────────────────────────────────────────────── */

struct vm_area {
    virt_addr_t  start;
    virt_addr_t  end;
    u32          flags;
    struct vm_area *next;
};

#endif /* KERNEL_MM_H */
