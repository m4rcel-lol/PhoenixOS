#include "../include/kernel.h"
#include "../include/mm.h"

/* ── Bitmap-based physical page allocator ────────────────────────────────── */

#define PMM_MAX_PAGES  (1024 * 1024)   /* support up to 4 GB */
#define BITMAP_WORDS   (PMM_MAX_PAGES / 64)

static u64  bitmap[BITMAP_WORDS];      /* 1 bit per page; 1 = free, 0 = used */
static u64  total_pages = 0;
static u64  free_pages  = 0;
static u64  bitmap_start_pfn = 0;     /* first PFN tracked */

/* ── Bit helpers ──────────────────────────────────────────────────────────── */

static inline void bitmap_set(u64 pfn) {
    u64 idx = pfn / 64;
    u64 bit = pfn % 64;
    bitmap[idx] |= (1ULL << bit);
}

static inline void bitmap_clear(u64 pfn) {
    u64 idx = pfn / 64;
    u64 bit = pfn % 64;
    bitmap[idx] &= ~(1ULL << bit);
}

static inline bool bitmap_test(u64 pfn) {
    u64 idx = pfn / 64;
    u64 bit = pfn % 64;
    return (bitmap[idx] >> bit) & 1;
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

void pmm_init(struct multiboot_mmap_entry *entries, u32 count) {
    /* Start with everything marked used */
    for (u32 i = 0; i < BITMAP_WORDS; i++)
        bitmap[i] = 0;

    /* Walk memory map, mark available regions free */
    for (u32 i = 0; i < count; i++) {
        struct multiboot_mmap_entry *e = &entries[i];
        if (e->type != 1) continue;   /* not available */

        u64 base = ALIGN_UP(e->base_addr, PAGE_SIZE);
        u64 end  = ALIGN_DOWN(e->base_addr + e->length, PAGE_SIZE);

        if (end <= base) continue;

        u64 pfn_start = base / PAGE_SIZE;
        u64 pfn_end   = end  / PAGE_SIZE;

        for (u64 pfn = pfn_start; pfn < pfn_end && pfn < PMM_MAX_PAGES; pfn++) {
            bitmap_set(pfn);
            total_pages++;
            free_pages++;
        }
    }

    /* Mark the first 1 MB as used (BIOS, VGA, etc.) */
    for (u64 pfn = 0; pfn < 256; pfn++) {
        if (bitmap_test(pfn)) {
            bitmap_clear(pfn);
            free_pages--;
        }
    }

    /* Mark kernel image pages as used (0x100000 – kernel_end) */
    extern u8 kernel_end[];
    u64 kend_phys = VIRT_TO_PHYS((u64)kernel_end);
    u64 kpfn_end  = ALIGN_UP(kend_phys, PAGE_SIZE) / PAGE_SIZE;
    for (u64 pfn = KERNEL_PHYS / PAGE_SIZE; pfn < kpfn_end && pfn < PMM_MAX_PAGES; pfn++) {
        if (bitmap_test(pfn)) {
            bitmap_clear(pfn);
            free_pages--;
        }
    }

    /* Mark PMM bitmap itself as used */
    u64 bm_phys_start = VIRT_TO_PHYS((u64)bitmap);
    u64 bm_phys_end   = bm_phys_start + sizeof(bitmap);
    for (u64 pfn = bm_phys_start / PAGE_SIZE;
         pfn < ALIGN_UP(bm_phys_end, PAGE_SIZE) / PAGE_SIZE && pfn < PMM_MAX_PAGES;
         pfn++) {
        if (bitmap_test(pfn)) {
            bitmap_clear(pfn);
            free_pages--;
        }
    }
}

/* ── Allocate a page ──────────────────────────────────────────────────────── */

phys_addr_t alloc_page(void) {
    /* Simple linear scan — fast enough for early boot, improve later */
    for (u64 word = 0; word < BITMAP_WORDS; word++) {
        if (bitmap[word] == 0) continue;
        /* Find lowest set bit */
        u64 bit = __builtin_ctzll(bitmap[word]);
        u64 pfn = word * 64 + bit;
        bitmap_clear(pfn);
        free_pages--;
        phys_addr_t addr = pfn * PAGE_SIZE;
        /* Zero the page */
        u64 *p = (u64 *)PHYS_TO_VIRT(addr);
        for (int i = 0; i < (int)(PAGE_SIZE / sizeof(u64)); i++) p[i] = 0;
        return addr;
    }
    panic("pmm: out of physical memory!\n");
    return 0;
}

/* ── Free a page ──────────────────────────────────────────────────────────── */

void free_page(phys_addr_t addr) {
    if (!IS_ALIGNED(addr, PAGE_SIZE)) {
        printk("pmm: free_page: unaligned address 0x%lx\n", addr);
        return;
    }
    u64 pfn = addr / PAGE_SIZE;
    if (pfn >= PMM_MAX_PAGES) {
        printk("pmm: free_page: pfn %lu out of range\n", pfn);
        return;
    }
    if (bitmap_test(pfn)) {
        printk("pmm: free_page: double free at 0x%lx\n", addr);
        return;
    }
    bitmap_set(pfn);
    free_pages++;
}

/* ── Stats ────────────────────────────────────────────────────────────────── */

u64 pmm_get_free_pages(void)  { return free_pages; }
u64 pmm_get_total_pages(void) { return total_pages; }

void pmm_print_stats(void) {
    u64 free_mb  = (free_pages  * PAGE_SIZE) / (1024 * 1024);
    u64 total_mb = (total_pages * PAGE_SIZE) / (1024 * 1024);
    printk("[pmm ] %lu MB free / %lu MB total (%lu pages free)\n",
           free_mb, total_mb, free_pages);
}
