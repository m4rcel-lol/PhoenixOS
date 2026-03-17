#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include "kernel.h"

/* ── PIT / timer globals ──────────────────────────────────────────────────── */

extern volatile u64 timer_ticks;  /* incremented every IRQ0 */
extern u32          ticks_per_ms; /* timer ticks per millisecond */

/* ── Interface ────────────────────────────────────────────────────────────── */

void pit_init(u32 frequency);
u64  get_ticks(void);
void sleep_ms(u32 ms);

#endif /* KERNEL_TIMER_H */
