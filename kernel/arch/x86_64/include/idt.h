#ifndef ARCH_X86_64_IDT_H
#define ARCH_X86_64_IDT_H

#include "../../../include/kernel.h"

/* ── IDT gate types ───────────────────────────────────────────────────────── */

#define IDT_INTERRUPT_GATE  0x8E   /* P=1 DPL=0 type=0xE (64-bit) */
#define IDT_TRAP_GATE       0x8F   /* P=1 DPL=0 type=0xF */
#define IDT_USER_GATE       0xEE   /* P=1 DPL=3 (accessible from ring-3) */

#define IDT_ENTRIES  256

/* ── IDT entry (16 bytes for 64-bit) ─────────────────────────────────────── */

struct idt_entry {
    u16 offset_low;    /* bits [15:0] of handler address */
    u16 selector;      /* code segment selector */
    u8  ist;           /* interrupt stack table index (0 = none) */
    u8  type_attr;     /* gate type + DPL + present */
    u16 offset_mid;    /* bits [31:16] */
    u32 offset_high;   /* bits [63:32] */
    u32 reserved;
} __packed;

/* ── IDT pointer ──────────────────────────────────────────────────────────── */

struct idt_ptr {
    u16 limit;
    u64 base;
} __packed;

/* ── Interrupt stack frame pushed by CPU + our stubs ─────────────────────── */

struct interrupt_frame {
    /* Saved by our stub (SAVE_REGS in startup.asm) */
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by our stub before SAVE_REGS */
    u64 int_no;
    u64 error_code;
    /* Pushed by CPU automatically */
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
} __packed;

/* ── Interface ────────────────────────────────────────────────────────────── */

void idt_init(void);
void idt_set_gate(u8 num, u64 handler, u8 type);

/* Dispatcher called from isr_common_stub */
void exception_dispatch(struct interrupt_frame *frame);

#endif /* ARCH_X86_64_IDT_H */
