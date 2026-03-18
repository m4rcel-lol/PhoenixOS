#include "../../include/kernel.h"
#include "../../include/net.h"
#include "../../include/pci.h"

/* ── Known WiFi adapter PCI vendor:device pairs ──────────────────────────── */

typedef struct { u16 vendor; u16 device; const char *name; } wifi_id_t;

static const wifi_id_t wifi_ids[] = {
    /* Intel Wireless */
    { 0x8086, 0x08B1, "Intel Wireless 7260" },
    { 0x8086, 0x095A, "Intel Wireless 7265" },
    { 0x8086, 0x24F3, "Intel Wireless 8260" },
    { 0x8086, 0x2526, "Intel Wi-Fi 6 AX200" },
    { 0x8086, 0x02F0, "Intel Wi-Fi 6 AX201" },
    /* Atheros / Qualcomm */
    { 0x168C, 0x003E, "Qualcomm Atheros QCA6174" },
    { 0x168C, 0x0042, "Qualcomm Atheros QCA9377" },
    /* Realtek */
    { 0x10EC, 0xB723, "Realtek RTL8723BE" },
    { 0x10EC, 0xC821, "Realtek RTL8821CE" },
    /* Broadcom */
    { 0x14E4, 0x43A0, "Broadcom BCM4360" },
    { 0x14E4, 0x43EC, "Broadcom BCM4356" },
};

#define WIFI_ID_COUNT  ((u32)(sizeof(wifi_ids) / sizeof(wifi_ids[0])))

/* ── Internal state ──────────────────────────────────────────────────────── */

static net_state_t   wifi_state = NET_STATE_NOT_FOUND;
static const char   *wifi_name  = NULL;
static pci_device_t  wifi_pci;

/* ── PCI enumeration callback ────────────────────────────────────────────── */

typedef struct { bool found; } wifi_ctx_t;

static void wifi_pci_cb(const pci_device_t *d, void *arg) {
    wifi_ctx_t *ctx = (wifi_ctx_t *)arg;
    if (ctx->found) return;

    /* Match on PCI network class / WiFi subclass first */
    bool class_match = (d->class_code == PCI_CLASS_NETWORK &&
                        d->subclass   == PCI_SUBCLASS_WIFI);

    /* Also check known device IDs regardless of subclass */
    bool id_match = false;
    for (u32 i = 0; i < WIFI_ID_COUNT; i++) {
        if (d->vendor_id == wifi_ids[i].vendor &&
            d->device_id == wifi_ids[i].device) {
            wifi_name = wifi_ids[i].name;
            id_match  = true;
            break;
        }
    }

    if (class_match || id_match) {
        wifi_pci  = *d;
        ctx->found = true;
    }
}

/* ── wifi_init ───────────────────────────────────────────────────────────── */

void wifi_init(void) {
    wifi_state = NET_STATE_NOT_FOUND;
    wifi_name  = NULL;

    wifi_ctx_t ctx = { false };
    pci_enumerate(wifi_pci_cb, &ctx);

    if (!ctx.found) {
        printk("[wifi] No supported WiFi adapter found\n");
        return;
    }

    if (!wifi_name)
        wifi_name = "Generic 802.11 adapter";

    printk("[wifi] Detected: %s (PCI %04x:%04x, IRQ %d)\n",
           wifi_name,
           wifi_pci.vendor_id,
           wifi_pci.device_id,
           wifi_pci.irq_line);

    /* Enable bus-mastering for DMA */
    u16 cmd = pci_read16(wifi_pci.bus, wifi_pci.dev, wifi_pci.func,
                         PCI_COMMAND);
    cmd |= 0x04;   /* Bus Master Enable */
    pci_write16(wifi_pci.bus, wifi_pci.dev, wifi_pci.func,
                PCI_COMMAND, cmd);

    wifi_state = NET_STATE_DISABLED;
    printk("[wifi] Driver loaded (firmware upload not yet implemented)\n");
}

/* ── wifi_get_state ──────────────────────────────────────────────────────── */

net_state_t wifi_get_state(void) {
    return wifi_state;
}
