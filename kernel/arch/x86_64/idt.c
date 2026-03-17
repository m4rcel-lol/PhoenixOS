#include "../../include/kernel.h"
#include "include/idt.h"
#include "include/gdt.h"

/* ── IDT table ────────────────────────────────────────────────────────────── */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_ptr_val;

/* ── Exception names ──────────────────────────────────────────────────────── */

static const char *exception_names[32] = {
    "Division Error",          /* 0  */
    "Debug",                   /* 1  */
    "Non-maskable Interrupt",  /* 2  */
    "Breakpoint",              /* 3  */
    "Overflow",                /* 4  */
    "Bound Range Exceeded",    /* 5  */
    "Invalid Opcode",          /* 6  */
    "Device Not Available",    /* 7  */
    "Double Fault",            /* 8  */
    "Coprocessor Segment Overrun", /* 9 */
    "Invalid TSS",             /* 10 */
    "Segment Not Present",     /* 11 */
    "Stack-Segment Fault",     /* 12 */
    "General Protection Fault",/* 13 */
    "Page Fault",              /* 14 */
    "Reserved",                /* 15 */
    "x87 FPU Error",           /* 16 */
    "Alignment Check",         /* 17 */
    "Machine Check",           /* 18 */
    "SIMD FP Exception",       /* 19 */
    "Virtualization Exception",/* 20 */
    "Reserved",                /* 21 */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved",
    "Security Exception",      /* 30 */
    "Reserved",                /* 31 */
};

/* ── External ISR / IRQ stubs (from startup.asm) ─────────────────────────── */

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

/* ── Set one IDT gate ─────────────────────────────────────────────────────── */

void idt_set_gate(u8 num, u64 handler, u8 type) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].selector    = KERNEL_CS;
    idt[num].ist         = 0;
    idt[num].type_attr   = type;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].reserved    = 0;
}

/* ── idt_init ─────────────────────────────────────────────────────────────── */

void idt_init(void) {
    /* Exceptions 0-31 */
    idt_set_gate(0,  (u64)isr0,  IDT_INTERRUPT_GATE);
    idt_set_gate(1,  (u64)isr1,  IDT_TRAP_GATE);
    idt_set_gate(2,  (u64)isr2,  IDT_INTERRUPT_GATE);
    idt_set_gate(3,  (u64)isr3,  IDT_TRAP_GATE);
    idt_set_gate(4,  (u64)isr4,  IDT_TRAP_GATE);
    idt_set_gate(5,  (u64)isr5,  IDT_INTERRUPT_GATE);
    idt_set_gate(6,  (u64)isr6,  IDT_INTERRUPT_GATE);
    idt_set_gate(7,  (u64)isr7,  IDT_INTERRUPT_GATE);
    idt_set_gate(8,  (u64)isr8,  IDT_INTERRUPT_GATE);
    idt_set_gate(9,  (u64)isr9,  IDT_INTERRUPT_GATE);
    idt_set_gate(10, (u64)isr10, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (u64)isr11, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (u64)isr12, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (u64)isr13, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (u64)isr14, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (u64)isr15, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (u64)isr16, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (u64)isr17, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (u64)isr18, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (u64)isr19, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (u64)isr20, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (u64)isr21, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (u64)isr22, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (u64)isr23, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (u64)isr24, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (u64)isr25, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (u64)isr26, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (u64)isr27, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (u64)isr28, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (u64)isr29, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (u64)isr30, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (u64)isr31, IDT_INTERRUPT_GATE);

    /* IRQ 0-15 mapped to vectors 32-47 */
    idt_set_gate(32, (u64)irq0,  IDT_INTERRUPT_GATE);
    idt_set_gate(33, (u64)irq1,  IDT_INTERRUPT_GATE);
    idt_set_gate(34, (u64)irq2,  IDT_INTERRUPT_GATE);
    idt_set_gate(35, (u64)irq3,  IDT_INTERRUPT_GATE);
    idt_set_gate(36, (u64)irq4,  IDT_INTERRUPT_GATE);
    idt_set_gate(37, (u64)irq5,  IDT_INTERRUPT_GATE);
    idt_set_gate(38, (u64)irq6,  IDT_INTERRUPT_GATE);
    idt_set_gate(39, (u64)irq7,  IDT_INTERRUPT_GATE);
    idt_set_gate(40, (u64)irq8,  IDT_INTERRUPT_GATE);
    idt_set_gate(41, (u64)irq9,  IDT_INTERRUPT_GATE);
    idt_set_gate(42, (u64)irq10, IDT_INTERRUPT_GATE);
    idt_set_gate(43, (u64)irq11, IDT_INTERRUPT_GATE);
    idt_set_gate(44, (u64)irq12, IDT_INTERRUPT_GATE);
    idt_set_gate(45, (u64)irq13, IDT_INTERRUPT_GATE);
    idt_set_gate(46, (u64)irq14, IDT_INTERRUPT_GATE);
    idt_set_gate(47, (u64)irq15, IDT_INTERRUPT_GATE);

    /* Load IDTR */
    idt_ptr_val.limit = sizeof(idt) - 1;
    idt_ptr_val.base  = (u64)&idt;
    __asm__ volatile("lidt %0" :: "m"(idt_ptr_val) : "memory");
}

/* ── Exception dispatcher ─────────────────────────────────────────────────── */

void exception_dispatch(struct interrupt_frame *frame) {
    u64 int_no = frame->int_no;

    /* Special handling for page fault: read CR2 */
    if (int_no == 14) {
        u64 fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        printk("\n[EXC] #PF Page Fault at RIP=0x%lx  addr=0x%lx  err=0x%lx\n",
               frame->rip, fault_addr, frame->error_code);
        printk("      %s %s %s\n",
               frame->error_code & 1 ? "protection" : "not-present",
               frame->error_code & 2 ? "write" : "read",
               frame->error_code & 4 ? "user" : "kernel");
        goto dump_and_panic;
    }

    /* Breakpoint — don't panic, just log */
    if (int_no == 3) {
        printk("[EXC] Breakpoint at RIP=0x%lx\n", frame->rip);
        return;
    }

    printk("\n[EXC] Exception %lu: %s\n",
           int_no,
           int_no < 32 ? exception_names[int_no] : "Unknown");
    printk("      Error code: 0x%lx\n", frame->error_code);

dump_and_panic:
    printk("      RIP=0x%016lx  CS=0x%lx  RFLAGS=0x%lx\n",
           frame->rip, frame->cs, frame->rflags);
    printk("      RSP=0x%016lx  SS=0x%lx\n",
           frame->rsp, frame->ss);
    printk("      RAX=0x%016lx  RBX=0x%016lx\n", frame->rax, frame->rbx);
    printk("      RCX=0x%016lx  RDX=0x%016lx\n", frame->rcx, frame->rdx);
    printk("      RSI=0x%016lx  RDI=0x%016lx\n", frame->rsi, frame->rdi);
    printk("      R8 =0x%016lx  R9 =0x%016lx\n", frame->r8,  frame->r9);
    printk("      R10=0x%016lx  R11=0x%016lx\n", frame->r10, frame->r11);
    printk("      R12=0x%016lx  R13=0x%016lx\n", frame->r12, frame->r13);
    printk("      R14=0x%016lx  R15=0x%016lx\n", frame->r14, frame->r15);

    panic("Unhandled exception — system halted.\n");
}
