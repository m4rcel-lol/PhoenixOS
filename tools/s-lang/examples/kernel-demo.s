/* kernel-demo.s — OS/kernel programming demo in S
 *
 * Compile:  sc kernel-demo.s -o kernel-demo
 * Run:      ./kernel-demo
 *
 * Demonstrates S features useful for OS/systems programming:
 *   - Bit manipulation macros
 *   - Memory layout simulation
 *   - Type aliases (u8, u16, u32, u64, usize)
 *   - Structs
 *   - Pointer arithmetic
 *   - S_BIT, S_ARRAY_LEN, S_ALIGN_UP macros
 */

/* ── Page / Memory constants ──────────────────────────────────────────────── */

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12
#define MEM_BASE    0x100000  /* 1 MiB */

/* ── Physical page descriptor ────────────────────────────────────────────── */

typedef struct {
    u64   phys_addr;
    u32   flags;
    u32   ref_count;
} PageDesc;

#define PAGE_FLAG_FREE    S_BIT(0)
#define PAGE_FLAG_KERNEL  S_BIT(1)
#define PAGE_FLAG_USER    S_BIT(2)
#define PAGE_FLAG_RO      S_BIT(3)

/* ── Simple bitmap allocator ─────────────────────────────────────────────── */

#define POOL_PAGES  64
static u64  pool_bitmap = 0;  /* 1 = free, 0 = used */
static u64  pool_base   = MEM_BASE;

fn pmm_init() -> void {
    /* Mark all pages free */
    pool_bitmap = 0xFFFFFFFFFFFFFFFFULL;
    println("[PMM] Initialised %d-page pool at 0x%llx", POOL_PAGES, (unsigned long long)pool_base);
}

fn pmm_alloc() -> u64 {
    if (pool_bitmap == 0) {
        eprintln("[PMM] Out of memory!");
        return 0;
    }
    /* Find lowest set bit */
    var i32 idx = __builtin_ctzll(pool_bitmap);
    pool_bitmap &= ~S_BIT64(idx);
    var u64 addr = pool_base + (u64)idx * PAGE_SIZE;
    return addr;
}

fn pmm_free(u64 addr) -> void {
    if (addr < pool_base) return;
    var u64 offset = addr - pool_base;
    var i32 idx = (i32)(offset / PAGE_SIZE);
    if (idx < 0 || idx >= POOL_PAGES) return;
    pool_bitmap |= S_BIT64(idx);
}

fn pmm_free_count() -> i32 {
    return __builtin_popcountll(pool_bitmap);
}

/* ── Virtual address utilities ────────────────────────────────────────────── */

fn vaddr_to_pml4(u64 va) -> i32 { return (i32)((va >> 39) & 0x1FF); }
fn vaddr_to_pdpt(u64 va) -> i32 { return (i32)((va >> 30) & 0x1FF); }
fn vaddr_to_pd(u64 va)   -> i32 { return (i32)((va >> 21) & 0x1FF); }
fn vaddr_to_pt(u64 va)   -> i32 { return (i32)((va >> 12) & 0x1FF); }
fn vaddr_offset(u64 va)  -> i32 { return (i32)(va & 0xFFF); }

fn dump_vaddr(u64 va) -> void {
    println("VA 0x%016llx -> PML4[%3d] PDPT[%3d] PD[%3d] PT[%3d] off[%4d]",
        (unsigned long long)va,
        vaddr_to_pml4(va), vaddr_to_pdpt(va),
        vaddr_to_pd(va),   vaddr_to_pt(va),
        vaddr_offset(va));
}

/* ── Alignment demo ───────────────────────────────────────────────────────── */

fn alignment_demo() -> void {
    println("\n--- Alignment ---");
    var usize sizes[] = {1, 3, 7, 8, 15, 16, 100, 4095, 4096, 4097};
    for i in 0..(i32)S_ARRAY_LEN(sizes) {
        var usize s = sizes[i];
        println("  align_up(%5zu, 4096) = %zu", s, S_ALIGN_UP(s, PAGE_SIZE));
    }
}

/* ── Register dump simulation ─────────────────────────────────────────────── */

typedef struct {
    u64 rax, rbx, rcx, rdx;
    u64 rsi, rdi, rbp, rsp;
    u64 r8,  r9,  r10, r11;
    u64 r12, r13, r14, r15;
    u64 rip, rflags;
} Registers;

fn dump_regs(Registers *regs) -> void {
    println("Register dump:");
    println("  RAX=0x%016llx  RBX=0x%016llx", (unsigned long long)regs->rax, (unsigned long long)regs->rbx);
    println("  RCX=0x%016llx  RDX=0x%016llx", (unsigned long long)regs->rcx, (unsigned long long)regs->rdx);
    println("  RSI=0x%016llx  RDI=0x%016llx", (unsigned long long)regs->rsi, (unsigned long long)regs->rdi);
    println("  RBP=0x%016llx  RSP=0x%016llx", (unsigned long long)regs->rbp, (unsigned long long)regs->rsp);
    println("  RIP=0x%016llx  RFLAGS=0x%016llx", (unsigned long long)regs->rip, (unsigned long long)regs->rflags);
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

fn main() -> i32 {
    println("=== PhoenixOS Kernel Demo (S language) ===\n");

    /* PMM demo */
    pmm_init();
    println("[PMM] Free pages: %d / %d", pmm_free_count(), POOL_PAGES);

    /* Allocate some pages */
    var u64 p1 = pmm_alloc();
    var u64 p2 = pmm_alloc();
    var u64 p3 = pmm_alloc();
    println("[PMM] Allocated: 0x%llx, 0x%llx, 0x%llx",
        (unsigned long long)p1, (unsigned long long)p2, (unsigned long long)p3);
    println("[PMM] Free pages: %d / %d", pmm_free_count(), POOL_PAGES);

    /* Free one */
    pmm_free(p2);
    println("[PMM] Freed 0x%llx, free pages: %d", (unsigned long long)p2, pmm_free_count());

    /* Virtual address breakdown */
    println("\n--- Virtual Address Breakdown ---");
    var u64 addrs[] = {
        0x0000000000001000ULL,   /* low user page */
        0x00007FFF12345678ULL,   /* user stack region */
        0xFFFF800000000000ULL,   /* kernel direct map */
        0xFFFFFFFF80000000ULL,   /* kernel -2GB */
    };
    for i in 0..(i32)S_ARRAY_LEN(addrs) {
        dump_vaddr(addrs[i]);
    }

    alignment_demo();

    /* Register dump (simulated) */
    println("\n--- Register Dump ---");
    Registers regs;
    s_zero(regs);
    regs.rax = 0xDEADBEEFCAFEBABEULL;
    regs.rip = 0xFFFFFFFF80010000ULL;
    regs.rsp = 0xFFFF800001000000ULL;
    regs.rflags = 0x0246; /* IF | PF | ZF */
    dump_regs(&regs);

    println("\n=== Demo complete ===");
    return 0;
}
