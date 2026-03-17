#ifndef ARCH_X86_64_ASM_H
#define ARCH_X86_64_ASM_H

#include <kernel/include/kernel.h>

/* ── I/O port access ──────────────────────────────────────────────────────── */

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline void outw(u16 port, u16 val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline void outl(u16 port, u32 val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline u8 inb(u16 port) {
    u8 val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline u16 inw(u16 port) {
    u16 val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

static inline u32 inl(u16 port) {
    u32 val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}

/* Short I/O delay via port 0x80 (POST diagnostic port, safe to write) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* ── Interrupt control ────────────────────────────────────────────────────── */

static inline void cli(void) {
    __asm__ volatile("cli" ::: "memory");
}

static inline void sti(void) {
    __asm__ volatile("sti" ::: "memory");
}

static inline void hlt(void) {
    __asm__ volatile("hlt");
}

/* Halt the CPU forever */
static inline void __attribute__((noreturn)) halt_forever(void) {
    for (;;) {
        cli();
        hlt();
    }
}

/* ── Memory barriers ──────────────────────────────────────────────────────── */

static inline void mb(void) {
    __asm__ volatile("mfence" ::: "memory");
}

static inline void rmb(void) {
    __asm__ volatile("lfence" ::: "memory");
}

static inline void wmb(void) {
    __asm__ volatile("sfence" ::: "memory");
}

static inline void barrier(void) {
    __asm__ volatile("" ::: "memory");
}

/* ── MSR access ───────────────────────────────────────────────────────────── */

static inline u64 rdmsr(u32 msr) {
    u32 lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | lo;
}

static inline void wrmsr(u32 msr, u64 val) {
    u32 lo = (u32)val;
    u32 hi = (u32)(val >> 32);
    __asm__ volatile("wrmsr" :: "c"(msr), "a"(lo), "d"(hi) : "memory");
}

/* ── CPUID ────────────────────────────────────────────────────────────────── */

static inline void cpuid(u32 leaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

/* ── CR register access ───────────────────────────────────────────────────── */

static inline u64 read_cr0(void) {
    u64 val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline u64 read_cr2(void) {
    u64 val;
    __asm__ volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

static inline u64 read_cr3(void) {
    u64 val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(u64 val) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(val) : "memory");
}

static inline u64 read_cr4(void) {
    u64 val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(u64 val) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(val) : "memory");
}

/* ── TLB ──────────────────────────────────────────────────────────────────── */

static inline void invlpg(virt_addr_t addr) {
    __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

static inline void flush_tlb(void) {
    write_cr3(read_cr3());
}

/* ── RFLAGS ───────────────────────────────────────────────────────────────── */

static inline u64 read_rflags(void) {
    u64 flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags));
    return flags;
}

/* ── Stack pointer ────────────────────────────────────────────────────────── */

static inline u64 read_rsp(void) {
    u64 rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(rsp));
    return rsp;
}

#endif /* ARCH_X86_64_ASM_H */
