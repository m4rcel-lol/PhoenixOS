#include "../include/kernel.h"
#include "arch/x86_64/include/gdt.h"
#include "arch/x86_64/include/asm.h"

/* ── GDT table ────────────────────────────────────────────────────────────── */

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr   gdt_ptr_val;
struct tss              kernel_tss;

/* ── Helper to set a GDT entry ────────────────────────────────────────────── */

static void gdt_set_entry(int idx, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[idx].base_low   = base & 0xFFFF;
    gdt[idx].base_mid   = (base >> 16) & 0xFF;
    gdt[idx].base_high  = (base >> 24) & 0xFF;
    gdt[idx].limit_low  = limit & 0xFFFF;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access     = access;
}

/* ── TSS descriptor occupies two GDT slots (128-bit system descriptor) ────── */

static void gdt_set_tss(int idx, u64 base, u32 limit) {
    /* Low 64 bits */
    u64 *g = (u64 *)&gdt[idx];
    g[0] = 0;
    g[0] |= (u64)(limit & 0xFFFF);
    g[0] |= (u64)(base & 0xFFFFFF) << 16;
    g[0] |= (u64)0x89 << 40;         /* type=TSS available, P=1, DPL=0 */
    g[0] |= (u64)((limit >> 16) & 0xF) << 48;
    g[0] |= (u64)((base >> 24) & 0xFF) << 56;
    /* High 64 bits (upper 32 bits of base) */
    g[1] = (base >> 32) & 0xFFFFFFFF;
}

/* ── gdt_init ─────────────────────────────────────────────────────────────── */

void gdt_init(void) {
    /* 0x00: null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 0x08: kernel code (64-bit) — L bit in granularity */
    gdt_set_entry(1, 0, 0xFFFFF,
        0x9A,   /* P=1 DPL=0 S=1 type=0xA (code, execute/read) */
        0xA0);  /* G=1 L=1 */

    /* 0x10: kernel data */
    gdt_set_entry(2, 0, 0xFFFFF,
        0x92,   /* P=1 DPL=0 S=1 type=0x2 (data, read/write) */
        0xC0);  /* G=1 D/B=1 */

    /* 0x18: user code (64-bit) DPL=3 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    /* 0x20: user data DPL=3 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);

    /* 0x28: TSS descriptor (two consecutive 64-bit entries) */
    gdt_set_tss(5, (u64)&kernel_tss, sizeof(kernel_tss) - 1);

    /* Load GDTR */
    gdt_ptr_val.limit = sizeof(gdt) - 1;
    gdt_ptr_val.base  = (u64)&gdt;

    __asm__ volatile(
        "lgdt %0\n"
        /* Reload CS via a far return trick */
        "pushq %1\n"
        "lea   1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        /* Reload data segments */
        "mov %2, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "m"(gdt_ptr_val), "i"((u64)KERNEL_CS), "i"((u16)KERNEL_DS)
        : "rax", "memory"
    );
}

/* ── tss_init ─────────────────────────────────────────────────────────────── */

void tss_init(void) {
    /* Allocate a kernel stack for ring-0 entry (4KB) */
    static u8 tss_stack[4096] __aligned(16);
    kernel_tss.rsp0       = (u64)&tss_stack[sizeof(tss_stack)];
    kernel_tss.iopb_offset = sizeof(struct tss);
}

void tss_set_rsp0(u64 rsp) {
    kernel_tss.rsp0 = rsp;
}

/* ── load_tss ─────────────────────────────────────────────────────────────── */

void load_tss(void) {
    __asm__ volatile("ltr %0" :: "r"((u16)TSS_SEL) : "memory");
}
