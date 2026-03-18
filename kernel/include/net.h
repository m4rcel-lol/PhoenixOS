#ifndef KERNEL_NET_H
#define KERNEL_NET_H

#include "kernel.h"

/* ── Generic network driver state ────────────────────────────────────────── */

typedef enum {
    NET_STATE_NOT_FOUND = 0,  /* no hardware detected */
    NET_STATE_DISABLED,       /* hardware present but not yet enabled */
    NET_STATE_ENABLED,        /* driver initialized, radio/link up */
    NET_STATE_CONNECTED,      /* associated / link established */
} net_state_t;

/* ── WiFi public API ─────────────────────────────────────────────────────── */

void        wifi_init(void);
net_state_t wifi_get_state(void);

/* ── Bluetooth public API ────────────────────────────────────────────────── */

void        bluetooth_init(void);
net_state_t bluetooth_get_state(void);

#endif /* KERNEL_NET_H */
