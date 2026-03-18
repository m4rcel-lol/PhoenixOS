#include "../../include/kernel.h"
#include "../../include/net.h"
#include "../../include/pci.h"

/* ── Known Bluetooth PCI vendor:device pairs ─────────────────────────────── */

typedef struct { u16 vendor; u16 device; const char *name; } bt_id_t;

static const bt_id_t bt_ids[] = {
    /* Intel */
    { 0x8086, 0x0A2A, "Intel Bluetooth (Broadwell)" },
    { 0x8086, 0x3198, "Intel Bluetooth (Gemini Lake)" },
    { 0x8086, 0x02D0, "Intel AX201 Bluetooth" },
    { 0x8086, 0x0026, "Intel AX200 Bluetooth" },
    /* Qualcomm Atheros */
    { 0x168C, 0x0036, "Qualcomm Atheros AR3012 Bluetooth" },
    /* Realtek */
    { 0x10EC, 0xB009, "Realtek RTL8822BE Bluetooth" },
    /* Broadcom */
    { 0x0A5C, 0x216C, "Broadcom BCM2045 Bluetooth" },
    { 0x0A5C, 0x21E6, "Broadcom BCM20702 Bluetooth" },
};

#define BT_ID_COUNT  ((u32)(sizeof(bt_ids) / sizeof(bt_ids[0])))

/* ── Internal state ──────────────────────────────────────────────────────── */

static net_state_t   bt_state = NET_STATE_NOT_FOUND;
static const char   *bt_name  = NULL;
static pci_device_t  bt_pci;

/* ── PCI enumeration callback ────────────────────────────────────────────── */

typedef struct { bool found; } bt_ctx_t;

static void bt_pci_cb(const pci_device_t *d, void *arg) {
    bt_ctx_t *ctx = (bt_ctx_t *)arg;
    if (ctx->found) return;

    /* Bluetooth controllers appear under the serial-bus class */
    bool class_match = (d->class_code == PCI_CLASS_SERIAL &&
                        d->subclass   == PCI_SUBCLASS_BLUETOOTH);

    /* Also match against known device IDs */
    bool id_match = false;
    for (u32 i = 0; i < BT_ID_COUNT; i++) {
        if (d->vendor_id == bt_ids[i].vendor &&
            d->device_id == bt_ids[i].device) {
            bt_name   = bt_ids[i].name;
            id_match  = true;
            break;
        }
    }

    if (class_match || id_match) {
        bt_pci     = *d;
        ctx->found = true;
    }
}

/* ── bluetooth_init ──────────────────────────────────────────────────────── */

void bluetooth_init(void) {
    bt_state = NET_STATE_NOT_FOUND;
    bt_name  = NULL;

    bt_ctx_t ctx = { false };
    pci_enumerate(bt_pci_cb, &ctx);

    if (!ctx.found) {
        printk("[bt  ] No supported Bluetooth adapter found\n");
        return;
    }

    if (!bt_name)
        bt_name = "Generic Bluetooth controller";

    printk("[bt  ] Detected: %s (PCI %04x:%04x, IRQ %d)\n",
           bt_name,
           bt_pci.vendor_id,
           bt_pci.device_id,
           bt_pci.irq_line);

    /* Enable bus-mastering for DMA */
    u16 cmd = pci_read16(bt_pci.bus, bt_pci.dev, bt_pci.func, PCI_COMMAND);
    cmd |= 0x04;   /* Bus Master Enable */
    pci_write16(bt_pci.bus, bt_pci.dev, bt_pci.func, PCI_COMMAND, cmd);

    bt_state = NET_STATE_DISABLED;
    printk("[bt  ] Driver loaded (HCI stack not yet implemented)\n");
}

/* ── bluetooth_get_state ─────────────────────────────────────────────────── */

net_state_t bluetooth_get_state(void) {
    return bt_state;
}
