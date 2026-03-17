#include "../../include/kernel.h"
#include "include/idt.h"
#include "include/asm.h"

/* ── 8259A PIC constants ──────────────────────────────────────────────────── */

#define PIC1_CMD   0x20
#define PIC1_DATA  0x21
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

#define PIC_EOI    0x20

/* Initialization Command Words */
#define ICW1_ICW4  0x01   /* ICW4 needed */
#define ICW1_INIT  0x10
#define ICW4_8086  0x01

#define IRQ_BASE   32     /* PIC IRQ0 mapped to vector 32 */
#define IRQ_COUNT  16

/* ── IRQ handler table ────────────────────────────────────────────────────── */

typedef void (*irq_handler_t)(struct interrupt_frame *);

static irq_handler_t irq_handlers[IRQ_COUNT];

/* ── PIC initialization ───────────────────────────────────────────────────── */

static void pic_init(void) {
    /* Save masks */
    u8 mask1 = inb(PIC1_DATA);
    u8 mask2 = inb(PIC2_DATA);

    /* Cascade init sequence */
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);  io_wait();
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);  io_wait();
    outb(PIC1_DATA, IRQ_BASE);        io_wait();   /* master vector offset */
    outb(PIC2_DATA, IRQ_BASE + 8);    io_wait();   /* slave vector offset */
    outb(PIC1_DATA, 0x04);            io_wait();   /* master has slave at IRQ2 */
    outb(PIC2_DATA, 0x02);            io_wait();   /* slave cascade identity */
    outb(PIC1_DATA, ICW4_8086);       io_wait();
    outb(PIC2_DATA, ICW4_8086);       io_wait();

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

/* ── EOI ──────────────────────────────────────────────────────────────────── */

void pic_send_eoi(u8 irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* ── Enable / disable specific IRQ lines ─────────────────────────────────── */

void irq_enable(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  bit  = irq < 8 ? irq : irq - 8;
    outb(port, inb(port) & ~(1 << bit));
}

void irq_disable(u8 irq) {
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8  bit  = irq < 8 ? irq : irq - 8;
    outb(port, inb(port) | (1 << bit));
}

/* ── Install / remove handler ─────────────────────────────────────────────── */

void irq_install_handler(u8 irq, irq_handler_t handler) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = handler;
        irq_enable(irq);
    }
}

void irq_uninstall_handler(u8 irq) {
    if (irq < IRQ_COUNT) {
        irq_handlers[irq] = NULL;
        irq_disable(irq);
    }
}

/* ── Dispatch ─────────────────────────────────────────────────────────────── */

void irq_dispatch(struct interrupt_frame *frame) {
    u8 irq = (u8)(frame->int_no - IRQ_BASE);

    /* Check for spurious IRQ7 */
    if (irq == 7) {
        u8 isr;
        outb(PIC1_CMD, 0x0B);   /* read ISR */
        isr = inb(PIC1_CMD);
        if (!(isr & (1 << 7))) return;  /* spurious — don't send EOI */
    }
    /* Check for spurious IRQ15 */
    if (irq == 15) {
        u8 isr;
        outb(PIC2_CMD, 0x0B);
        isr = inb(PIC2_CMD);
        if (!(isr & (1 << 7))) {
            outb(PIC1_CMD, PIC_EOI);  /* must still EOI master for slave spurious */
            return;
        }
    }

    if (irq < IRQ_COUNT && irq_handlers[irq])
        irq_handlers[irq](frame);

    pic_send_eoi(irq);
}

/* ── Interrupts init ──────────────────────────────────────────────────────── */

void interrupts_init(void) {
    pic_init();
    /* Mask all IRQs initially; drivers enable their own */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    /* Unmask cascade (IRQ2) so slave PICs work */
    irq_enable(2);
}
