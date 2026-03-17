/* test_pmm.c — Unit tests for PhoenixOS Physical Memory Manager */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ── Minimal assert framework ─────────────────────────────────────────────── */

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

/* ── Stub PMM (mirrors kernel/mm/pmm.c logic) ─────────────────────────────── */

#define PAGE_SIZE       4096
#define MAX_PAGES       1024    /* 4 MB test pool */
#define BITMAP_WORDS    (MAX_PAGES / 64)

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t  u8;
typedef unsigned long phys_addr_t;

static u64  bitmap[BITMAP_WORDS];
static u32  total_pages = 0;
static u32  free_pages  = 0;
static phys_addr_t mem_base = 0x100000;  /* 1 MB base */

static void pmm_test_init(u32 num_pages) {
    memset(bitmap, 0xFF, sizeof(bitmap));  /* All used */
    total_pages = num_pages;
    free_pages  = 0;

    /* Mark num_pages as free */
    for (u32 i = 0; i < num_pages; i++) {
        u32 word = i / 64;
        u32 bit  = i % 64;
        bitmap[word] &= ~((u64)1 << bit);
        free_pages++;
    }
}

static phys_addr_t alloc_page(void) {
    for (u32 word = 0; word < BITMAP_WORDS; word++) {
        if (bitmap[word] == 0xFFFFFFFFFFFFFFFFULL) continue;
        for (u32 bit = 0; bit < 64; bit++) {
            if (!(bitmap[word] & ((u64)1 << bit))) {
                bitmap[word] |= ((u64)1 << bit);
                free_pages--;
                return mem_base + (u64)(word * 64 + bit) * PAGE_SIZE;
            }
        }
    }
    return 0;  /* OOM */
}

static void free_page(phys_addr_t addr) {
    if (addr < mem_base) return;
    u32 page_idx = (u32)((addr - mem_base) / PAGE_SIZE);
    if (page_idx >= MAX_PAGES) return;
    u32 word = page_idx / 64;
    u32 bit  = page_idx % 64;
    if (bitmap[word] & ((u64)1 << bit)) {
        bitmap[word] &= ~((u64)1 << bit);
        free_pages++;
    }
}

static u32 get_free_pages(void)  { return free_pages; }
static u32 get_total_pages(void) { return total_pages; }

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_init(void) {
    TEST("PMM initialization");
    pmm_test_init(256);
    ASSERT(get_total_pages() == 256);
    ASSERT(get_free_pages()  == 256);
}

static void test_single_alloc(void) {
    TEST("Single page allocation");
    pmm_test_init(64);

    u32 before = get_free_pages();
    phys_addr_t p = alloc_page();
    ASSERT(p != 0);
    ASSERT(p >= mem_base);
    ASSERT((p & (PAGE_SIZE - 1)) == 0);  /* Page-aligned */
    ASSERT(get_free_pages() == before - 1);
}

static void test_multiple_alloc(void) {
    TEST("Multiple page allocations");
    pmm_test_init(64);

    phys_addr_t pages[16];
    for (int i = 0; i < 16; i++) {
        pages[i] = alloc_page();
        ASSERT(pages[i] != 0);
    }

    /* All pages must be unique */
    for (int i = 0; i < 16; i++) {
        for (int j = i + 1; j < 16; j++) {
            ASSERT(pages[i] != pages[j]);
        }
    }
    ASSERT(get_free_pages() == 64 - 16);
}

static void test_free(void) {
    TEST("Page free and reuse");
    pmm_test_init(16);

    phys_addr_t p1 = alloc_page();
    phys_addr_t p2 = alloc_page();
    ASSERT(p1 != 0);
    ASSERT(p2 != 0);

    u32 free_before = get_free_pages();
    free_page(p1);
    ASSERT(get_free_pages() == free_before + 1);

    /* Should be able to re-allocate */
    phys_addr_t p3 = alloc_page();
    ASSERT(p3 == p1);  /* Should get the just-freed page back */
    ASSERT(get_free_pages() == free_before);
}

static void test_oom(void) {
    TEST("Out-of-memory detection");
    pmm_test_init(4);

    /* Exhaust all pages */
    for (int i = 0; i < 4; i++) {
        phys_addr_t p = alloc_page();
        ASSERT(p != 0);
    }
    ASSERT(get_free_pages() == 0);

    /* Next allocation should fail */
    phys_addr_t p = alloc_page();
    ASSERT(p == 0);
}

static void test_double_free_safety(void) {
    TEST("Double-free safety");
    pmm_test_init(8);
    phys_addr_t p = alloc_page();
    ASSERT(p != 0);

    u32 free_before = get_free_pages();
    free_page(p);
    ASSERT(get_free_pages() == free_before + 1);

    /* Double free must not corrupt counter */
    free_page(p);
    ASSERT(get_free_pages() == free_before + 1);
}

static void test_page_tracking(void) {
    TEST("Page count tracking");
    pmm_test_init(100);
    ASSERT(get_total_pages() == 100);
    ASSERT(get_free_pages()  == 100);

    for (int i = 0; i < 50; i++) alloc_page();
    ASSERT(get_free_pages() == 50);
    ASSERT(get_total_pages() == 100);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("=== EmberKernel PMM Unit Tests ===\n");

    test_init();
    test_single_alloc();
    test_multiple_alloc();
    test_free();
    test_oom();
    test_double_free_safety();
    test_page_tracking();

    printf("\n=== Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d FAILED", tests_failed);
    printf(" ===\n");

    return tests_failed == 0 ? 0 : 1;
}
