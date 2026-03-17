#ifndef ARCH_X86_64_GDT_H
#define ARCH_X86_64_GDT_H

#include <kernel/include/kernel.h>

/* ── Segment selector values ──────────────────────────────────────────────── */

#define KERNEL_CS    0x08
#define KERNEL_DS    0x10
#define USER_CS      0x1B    /* 0x18 | RPL=3 */
#define USER_DS      0x23    /* 0x20 | RPL=3 */
#define TSS_SEL      0x28

/* GDT entry count */
#define GDT_ENTRIES  7       /* null, kcode, kdata, ucode, udata, tss-low, tss-high */

/* ── GDT entry (8 bytes) ──────────────────────────────────────────────────── */

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;     /* limit_high [3:0] | flags [7:4] */
    u8  base_high;
} __packed;

/* ── GDT pointer ──────────────────────────────────────────────────────────── */

struct gdt_ptr {
    u16 limit;
    u64 base;
} __packed;

/* ── Task State Segment (minimal 64-bit TSS) ──────────────────────────────── */

struct tss {
    u32 reserved0;
    u64 rsp0;       /* kernel stack pointer for ring-3 -> ring-0 transition */
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist[7];     /* interrupt stack table */
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
} __packed;

/* ── Interface ────────────────────────────────────────────────────────────── */

void gdt_init(void);
void tss_init(void);
void tss_set_rsp0(u64 rsp);
void load_tss(void);

extern struct tss kernel_tss;

#endif /* ARCH_X86_64_GDT_H */
