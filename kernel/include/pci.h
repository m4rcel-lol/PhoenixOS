#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include "kernel.h"

/* ── PCI configuration-space register offsets ────────────────────────────── */

#define PCI_VENDOR_ID        0x00
#define PCI_DEVICE_ID        0x02
#define PCI_COMMAND          0x04
#define PCI_STATUS           0x06
#define PCI_REVISION_ID      0x08
#define PCI_PROG_IF          0x09
#define PCI_SUBCLASS         0x0A
#define PCI_CLASS_CODE       0x0B
#define PCI_CACHE_LINE_SIZE  0x0C
#define PCI_LATENCY_TIMER    0x0D
#define PCI_HEADER_TYPE      0x0E
#define PCI_BAR0             0x10
#define PCI_BAR1             0x14
#define PCI_BAR2             0x18
#define PCI_BAR3             0x1C
#define PCI_BAR4             0x20
#define PCI_BAR5             0x24
#define PCI_INTERRUPT_LINE   0x3C
#define PCI_INTERRUPT_PIN    0x3D

/* ── PCI class codes ─────────────────────────────────────────────────────── */

#define PCI_CLASS_NETWORK    0x02
#define PCI_CLASS_DISPLAY    0x03
#define PCI_CLASS_SERIAL     0x0C

/* PCI display subclasses */
#define PCI_SUBCLASS_VGA     0x00
#define PCI_SUBCLASS_XGA     0x01
#define PCI_SUBCLASS_3D      0x02
#define PCI_SUBCLASS_DISPLAY_OTHER 0x80

/* PCI network subclasses */
#define PCI_SUBCLASS_ETHERNET    0x00
#define PCI_SUBCLASS_WIFI        0x80

/* PCI serial-bus subclasses (used for USB/Bluetooth) */
#define PCI_SUBCLASS_USB         0x03
#define PCI_SUBCLASS_BLUETOOTH   0x11

/* Sentinel returned by config-space reads when slot is empty */
#define PCI_VENDOR_NONE  0xFFFF

/* ── PCI device descriptor ───────────────────────────────────────────────── */

typedef struct {
    u8  bus;
    u8  dev;
    u8  func;
    u16 vendor_id;
    u16 device_id;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  revision;
    u8  irq_line;
    u32 bar[6];
} pci_device_t;

/* ── Config-space access ─────────────────────────────────────────────────── */

u32  pci_read32 (u8 bus, u8 dev, u8 func, u8 offset);
u16  pci_read16 (u8 bus, u8 dev, u8 func, u8 offset);
u8   pci_read8  (u8 bus, u8 dev, u8 func, u8 offset);
void pci_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 val);
void pci_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 val);

/* ── Enumeration helpers ─────────────────────────────────────────────────── */

/* Scan all buses and call cb for every present function */
typedef void (*pci_enum_cb_t)(const pci_device_t *dev, void *arg);
void pci_enumerate(pci_enum_cb_t cb, void *arg);

/* Find the first device matching class_code / subclass.
 * Returns true and fills *out on success, false if not found. */
bool pci_find_by_class(u8 class_code, u8 subclass, pci_device_t *out);

#endif /* KERNEL_PCI_H */
