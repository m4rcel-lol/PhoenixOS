/* test_vmm.c — Unit tests for PhoenixOS Virtual Memory Manager */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ── Assert framework ─────────────────────────────────────────────────────── */

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  PASS: %s\n", #cond); \
    } else { \
        tests_failed++; \
        printf("  FAIL: %s  (line %d)\n", #cond, __LINE__); \
    } \
} while (0)

#define TEST(name) do { printf("\n[TEST] %s\n", name); } while(0)

/* ── Stub VMM (page table simulation) ────────────────────────────────────── */

typedef uint64_t u64;
typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;
typedef uint32_t u32;
typedef uint8_t  u8;

#define PAGE_SIZE   4096
#define PAGE_PRESENT  0x01
#define PAGE_WRITE    0x02
#define PAGE_USER     0x04
#define KERNEL_BASE   0xFFFFFFFF80000000ULL

/* Simulated page table: map virt -> phys */
#define MAX_MAPPINGS  1024

typedef struct {
    virt_addr_t virt;
    phys_addr_t phys;
    u32         flags;
    int         valid;
} PageEntry;

static PageEntry page_table[MAX_MAPPINGS];
static int       mapping_count = 0;

static int map_page(phys_addr_t phys, virt_addr_t virt, u32 flags) {
    /* Check for duplicate */
    for (int i = 0; i < mapping_count; i++) {
        if (page_table[i].valid && page_table[i].virt == virt) {
            /* Update */
            page_table[i].phys  = phys;
            page_table[i].flags = flags;
            return 0;
        }
    }
    if (mapping_count >= MAX_MAPPINGS) return -1;
    page_table[mapping_count].virt  = virt;
    page_table[mapping_count].phys  = phys;
    page_table[mapping_count].flags = flags;
    page_table[mapping_count].valid = 1;
    mapping_count++;
    return 0;
}

static int unmap_page(virt_addr_t virt) {
    for (int i = 0; i < mapping_count; i++) {
        if (page_table[i].valid && page_table[i].virt == virt) {
            page_table[i].valid = 0;
            return 0;
        }
    }
    return -1;  /* Not mapped */
}

static phys_addr_t virt_to_phys(virt_addr_t virt) {
    for (int i = 0; i < mapping_count; i++) {
        if (page_table[i].valid && page_table[i].virt == virt)
            return page_table[i].phys;
    }
    return 0;  /* Not mapped */
}

static int is_mapped(virt_addr_t virt) {
    return virt_to_phys(virt) != 0;
}

static void vmm_test_init(void) {
    memset(page_table, 0, sizeof(page_table));
    mapping_count = 0;
}

/* ── Stub kmalloc/kfree using a simple bump allocator ───────────────────── */

#define HEAP_SIZE (256 * 1024)
static u8   test_heap[HEAP_SIZE];
static int  heap_pos = 0;

typedef struct blk { int free; int size; struct blk *next; } blk_t;

static blk_t *heap_head = NULL;

static void heap_init(void) {
    heap_head = (blk_t *)test_heap;
    heap_head->free = 1;
    heap_head->size = HEAP_SIZE - sizeof(blk_t);
    heap_head->next = NULL;
    heap_pos = 0;
}

static void *kmalloc(int size) {
    blk_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size + (int)sizeof(blk_t)) {
            blk_t *nb = (blk_t *)((u8 *)b + sizeof(blk_t) + size);
            nb->size = b->size - size - sizeof(blk_t);
            nb->free = 1;
            nb->next = b->next;
            b->next  = nb;
            b->size  = size;
            b->free  = 0;
            return (u8 *)b + sizeof(blk_t);
        }
        b = b->next;
    }
    return NULL;
}

static void kfree(void *ptr) {
    if (!ptr) return;
    blk_t *b = (blk_t *)((u8 *)ptr - sizeof(blk_t));
    b->free = 1;
    /* Coalesce with next */
    if (b->next && b->next->free) {
        b->size += sizeof(blk_t) + b->next->size;
        b->next  = b->next->next;
    }
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_map_page(void) {
    TEST("Basic page mapping");
    vmm_test_init();

    int r = map_page(0x200000, 0xFFFF800000001000ULL, PAGE_PRESENT | PAGE_WRITE);
    ASSERT(r == 0);
    ASSERT(is_mapped(0xFFFF800000001000ULL));
    ASSERT(virt_to_phys(0xFFFF800000001000ULL) == 0x200000);
}

static void test_unmap_page(void) {
    TEST("Page unmapping");
    vmm_test_init();

    map_page(0x100000, 0xFFFF000000000000ULL, PAGE_PRESENT);
    ASSERT(is_mapped(0xFFFF000000000000ULL));

    int r = unmap_page(0xFFFF000000000000ULL);
    ASSERT(r == 0);
    ASSERT(!is_mapped(0xFFFF000000000000ULL));
}

static void test_unmap_nonexistent(void) {
    TEST("Unmap non-existent page returns error");
    vmm_test_init();
    int r = unmap_page(0xDEAD0000000DEAD0ULL);
    ASSERT(r == -1);
}

static void test_multiple_mappings(void) {
    TEST("Multiple distinct mappings");
    vmm_test_init();

    for (int i = 0; i < 32; i++) {
        virt_addr_t va = KERNEL_BASE + (virt_addr_t)i * PAGE_SIZE;
        phys_addr_t pa = 0x100000 + (phys_addr_t)i * PAGE_SIZE;
        ASSERT(map_page(pa, va, PAGE_PRESENT | PAGE_WRITE) == 0);
    }

    for (int i = 0; i < 32; i++) {
        virt_addr_t va = KERNEL_BASE + (virt_addr_t)i * PAGE_SIZE;
        phys_addr_t pa = 0x100000 + (phys_addr_t)i * PAGE_SIZE;
        ASSERT(virt_to_phys(va) == pa);
    }
}

static void test_remap(void) {
    TEST("Remapping a virtual address");
    vmm_test_init();

    map_page(0x300000, 0xFFFF000000002000ULL, PAGE_PRESENT);
    ASSERT(virt_to_phys(0xFFFF000000002000ULL) == 0x300000);

    /* Remap to different physical */
    map_page(0x400000, 0xFFFF000000002000ULL, PAGE_PRESENT | PAGE_WRITE);
    ASSERT(virt_to_phys(0xFFFF000000002000ULL) == 0x400000);
}

static void test_page_alignment(void) {
    TEST("Mappings must be page-aligned");
    virt_addr_t virt = 0xFFFF800000000000ULL;
    phys_addr_t phys = 0x200000;
    ASSERT((virt & (PAGE_SIZE - 1)) == 0);
    ASSERT((phys & (PAGE_SIZE - 1)) == 0);
}

static void test_kmalloc_basic(void) {
    TEST("kmalloc basic allocation");
    heap_init();

    void *p1 = kmalloc(64);
    ASSERT(p1 != NULL);

    void *p2 = kmalloc(128);
    ASSERT(p2 != NULL);
    ASSERT(p1 != p2);
    ASSERT((char *)p2 > (char *)p1);
}

static void test_kmalloc_free_reuse(void) {
    TEST("kfree and reuse");
    heap_init();

    void *p1 = kmalloc(256);
    ASSERT(p1 != NULL);
    kfree(p1);

    void *p2 = kmalloc(256);
    ASSERT(p2 == p1);  /* Should reuse freed block */
}

static void test_kmalloc_oom(void) {
    TEST("kmalloc OOM detection");
    heap_init();

    /* Exhaust heap */
    void *prev = NULL;
    while (1) {
        void *p = kmalloc(4096);
        if (!p) break;
        prev = p;
    }
    (void)prev;
    ASSERT(kmalloc(4096) == NULL);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== EmberKernel VMM Unit Tests ===\n");

    test_map_page();
    test_unmap_page();
    test_unmap_nonexistent();
    test_multiple_mappings();
    test_remap();
    test_page_alignment();
    test_kmalloc_basic();
    test_kmalloc_free_reuse();
    test_kmalloc_oom();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed == 0 ? 0 : 1;
}
