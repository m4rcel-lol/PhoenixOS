#ifndef KERNEL_MOUSE_H
#define KERNEL_MOUSE_H

#include "kernel.h"

/* ── Mouse event ─────────────────────────────────────────────────────────── */

typedef struct {
    s8   dx;           /* horizontal delta (positive = right) */
    s8   dy;           /* vertical delta   (positive = up)    */
    bool btn_left;
    bool btn_right;
    bool btn_middle;
} mouse_event_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

void mouse_init(void);
bool mouse_pending(void);
bool mouse_read(mouse_event_t *evt);

#endif /* KERNEL_MOUSE_H */
