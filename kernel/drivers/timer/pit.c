#include "../../include/kernel.h"
#include "../../include/timer.h"
#include "../../arch/x86_64/include/asm.h"

/* ── PIT constants ────────────────────────────────────────────────────────── */

#define PIT_CHANNEL0  0x40
#define PIT_CHANNEL1  0x41
#define PIT_CHANNEL2  0x42
#define PIT_CMD       0x43
#define PIT_BASE_HZ   1193182UL

/* ── Tick counter ─────────────────────────────────────────────────────────── */

volatile u64 timer_ticks = 0;
u32 ticks_per_ms = 1;

/* ── Forward: scheduler tick ─────────────────────────────────────────────── */
extern void sched_tick(void);

/* ── IRQ0 handler ─────────────────────────────────────────────────────────── */

#include "../../arch/x86_64/include/idt.h"

static void timer_irq_handler(struct interrupt_frame *frame) {
    (void)frame;
    timer_ticks++;
    sched_tick();
}

/* ── PIT init ─────────────────────────────────────────────────────────────── */

void pit_init(u32 frequency) {
    if (frequency == 0) frequency = 100;

    u32 divisor = (u32)(PIT_BASE_HZ / frequency);
    if (divisor > 0xFFFF) divisor = 0xFFFF;
    if (divisor < 2)      divisor = 2;

    ticks_per_ms = frequency / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    /* Set channel 0, lobyte/hibyte, rate generator (mode 2) */
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    /* Install IRQ0 handler */
    extern void irq_install_handler(u8 irq, void (*handler)(struct interrupt_frame *));
    irq_install_handler(0, timer_irq_handler);

    printk("[pit ] Timer set to %u Hz (divisor=%u)\n", frequency, divisor);
}

/* ── Accessors ────────────────────────────────────────────────────────────── */

u64 get_ticks(void) {
    return timer_ticks;
}

void sleep_ms(u32 ms) {
    u64 end = timer_ticks + (u64)ms * ticks_per_ms;
    while (timer_ticks < end)
        __asm__ volatile("pause");
}
