#include "../include/kernel.h"
#include "../include/mm.h"
#include "../arch/x86_64/include/asm.h"

/* ── 4-level page table management ───────────────────────────────────────── */

#define PML4_IDX(v)  (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v)  (((v) >> 30) & 0x1FF)
#define PD_IDX(v)    (((v) >> 21) & 0x1FF)
#define PT_IDX(v)    (((v) >> 12) & 0x1FF)

#define PHYS_MASK    0x000FFFFFFFFFF000ULL

/* Kernel PML4 — set up during vmm_init */
static u64 *kernel_pml4 = NULL;

/* ── Get or allocate a page table entry ──────────────────────────────────── */

static u64 *get_or_alloc_table(u64 *parent, u32 idx, u32 flags) {
    if (!(parent[idx] & PF_PRESENT)) {
        phys_addr_t page = alloc_page();
        parent[idx] = page | PF_PRESENT | PF_WRITE | (flags & PF_USER);
    }
    phys_addr_t table_phys = parent[idx] & PHYS_MASK;
    return (u64 *)PHYS_TO_VIRT(table_phys);
}

/* ── Map one 4KB page ─────────────────────────────────────────────────────── */

int map_page(phys_addr_t phys, virt_addr_t virt, u32 flags) {
    u64 *pml4 = kernel_pml4;

    u64 *pdpt = get_or_alloc_table(pml4, PML4_IDX(virt), flags);
    u64 *pd   = get_or_alloc_table(pdpt, PDPT_IDX(virt), flags);
    u64 *pt   = get_or_alloc_table(pd,   PD_IDX(virt),   flags);

    u64 entry = (phys & PHYS_MASK) | PF_PRESENT;
    if (flags & PF_WRITE)  entry |= PF_WRITE;
    if (flags & PF_USER)   entry |= PF_USER;
    if (!(flags & VM_EXEC)) entry |= PF_NX;

    pt[PT_IDX(virt)] = entry;
    invlpg(virt);
    return 0;
}

/* ── Unmap one 4KB page ───────────────────────────────────────────────────── */

void unmap_page(virt_addr_t virt) {
    u64 *pml4 = kernel_pml4;

    u64 pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & PF_PRESENT)) return;
    u64 *pdpt = (u64 *)PHYS_TO_VIRT(pml4e & PHYS_MASK);

    u64 pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & PF_PRESENT)) return;
    u64 *pd = (u64 *)PHYS_TO_VIRT(pdpte & PHYS_MASK);

    u64 pde = pd[PD_IDX(virt)];
    if (!(pde & PF_PRESENT)) return;
    u64 *pt = (u64 *)PHYS_TO_VIRT(pde & PHYS_MASK);

    pt[PT_IDX(virt)] = 0;
    invlpg(virt);
}

u64 *get_pml4(void) { return kernel_pml4; }

/* ── VMM init ─────────────────────────────────────────────────────────────── */

void vmm_init(void) {
    /* Allocate kernel PML4 */
    phys_addr_t pml4_phys = alloc_page();
    kernel_pml4 = (u64 *)PHYS_TO_VIRT(pml4_phys);

    /* Copy the current CR3's higher-half entries (set up by boot.asm) */
    u64 *boot_pml4 = (u64 *)PHYS_TO_VIRT(read_cr3() & PHYS_MASK);
    for (int i = 256; i < 512; i++)
        kernel_pml4[i] = boot_pml4[i];

    /* Install our new PML4 */
    write_cr3(pml4_phys);

    printk("[vmm ] PML4 at phys 0x%lx\n", pml4_phys);
}

/* ── Simple kernel heap (free-list allocator) ─────────────────────────────── */

#define HEAP_START   (KERNEL_BASE + 0x10000000ULL)   /* 256 MB after base */
#define HEAP_MAX     (64ULL * 1024 * 1024)            /* 64 MB heap */
#define HEAP_MAGIC   0xDEADBEEF

struct heap_block {
    u32  magic;
    u32  size;       /* usable size (not including header) */
    bool free;
    struct heap_block *next;
    struct heap_block *prev;
};

static struct heap_block *heap_head = NULL;
static virt_addr_t heap_brk = HEAP_START;

static void heap_expand(usize bytes) {
    usize pages = ALIGN_UP(bytes, PAGE_SIZE) / PAGE_SIZE;
    for (usize i = 0; i < pages; i++) {
        phys_addr_t p = alloc_page();
        map_page(p, heap_brk, PF_WRITE | VM_EXEC);
        heap_brk += PAGE_SIZE;
        if (heap_brk - HEAP_START > HEAP_MAX)
            panic("kmalloc: kernel heap exhausted!\n");
    }
}

void *kmalloc(usize size) {
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);   /* 16-byte alignment */

    /* Initialize heap on first call */
    if (!heap_head) {
        heap_expand(PAGE_SIZE);
        heap_head = (struct heap_block *)HEAP_START;
        heap_head->magic  = HEAP_MAGIC;
        heap_head->size   = PAGE_SIZE - sizeof(struct heap_block);
        heap_head->free   = true;
        heap_head->next   = NULL;
        heap_head->prev   = NULL;
    }

    /* Find a free block (first fit) */
    struct heap_block *blk = heap_head;
    while (blk) {
        if (blk->free && blk->size >= size) {
            /* Split if large enough */
            if (blk->size >= size + sizeof(struct heap_block) + 32) {
                struct heap_block *new_blk =
                    (struct heap_block *)((u8 *)blk + sizeof(struct heap_block) + size);
                new_blk->magic = HEAP_MAGIC;
                new_blk->size  = blk->size - size - sizeof(struct heap_block);
                new_blk->free  = true;
                new_blk->next  = blk->next;
                new_blk->prev  = blk;
                if (blk->next) blk->next->prev = new_blk;
                blk->next = new_blk;
                blk->size = size;
            }
            blk->free = false;
            return (u8 *)blk + sizeof(struct heap_block);
        }
        blk = blk->next;
    }

    /* No suitable block — expand heap */
    usize expand = ALIGN_UP(size + sizeof(struct heap_block), PAGE_SIZE);
    virt_addr_t old_brk = heap_brk;
    heap_expand(expand);

    struct heap_block *new_blk = (struct heap_block *)old_brk;
    new_blk->magic = HEAP_MAGIC;
    new_blk->size  = expand - sizeof(struct heap_block);
    new_blk->free  = true;
    new_blk->next  = NULL;
    new_blk->prev  = NULL;

    /* Link to end of list */
    struct heap_block *last = heap_head;
    while (last->next) last = last->next;
    last->next     = new_blk;
    new_blk->prev  = last;

    return kmalloc(size);  /* retry */
}

void *kzmalloc(usize size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        u8 *p = (u8 *)ptr;
        for (usize i = 0; i < size; i++) p[i] = 0;
    }
    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;
    struct heap_block *blk =
        (struct heap_block *)((u8 *)ptr - sizeof(struct heap_block));
    if (blk->magic != HEAP_MAGIC) {
        printk("kfree: corrupt block at %p\n", ptr);
        return;
    }
    blk->free = true;

    /* Coalesce with next */
    if (blk->next && blk->next->free) {
        blk->size += sizeof(struct heap_block) + blk->next->size;
        blk->next  = blk->next->next;
        if (blk->next) blk->next->prev = blk;
    }
    /* Coalesce with prev */
    if (blk->prev && blk->prev->free) {
        blk->prev->size += sizeof(struct heap_block) + blk->size;
        blk->prev->next  = blk->next;
        if (blk->next) blk->next->prev = blk->prev;
    }
}

void *krealloc(void *ptr, usize new_size) {
    if (!ptr)      return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return NULL; }

    struct heap_block *blk =
        (struct heap_block *)((u8 *)ptr - sizeof(struct heap_block));
    if (blk->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    u8 *src = (u8 *)ptr;
    u8 *dst = (u8 *)new_ptr;
    usize copy = blk->size < new_size ? blk->size : new_size;
    for (usize i = 0; i < copy; i++) dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}
