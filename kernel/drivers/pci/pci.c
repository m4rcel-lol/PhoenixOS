#include "../../include/kernel.h"
#include "../../include/pci.h"
#include "../../arch/x86_64/include/asm.h"

/* ── PCI mechanism #1 I/O ports ──────────────────────────────────────────── */

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

/* Build the 32-bit address value written to CONFIG_ADDR */
static inline u32 pci_addr(u8 bus, u8 dev, u8 func, u8 offset) {
    return (u32)(1U << 31)          /* enable bit */
         | ((u32)bus  << 16)
         | ((u32)(dev & 0x1F) << 11)
         | ((u32)(func & 0x07) << 8)
         | ((u32)(offset & 0xFC));  /* must be DWORD-aligned */
}

/* ── Config-space reads ──────────────────────────────────────────────────── */

u32 pci_read32(u8 bus, u8 dev, u8 func, u8 offset) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, dev, func, offset));
    return inl(PCI_CONFIG_DATA);
}

u16 pci_read16(u8 bus, u8 dev, u8 func, u8 offset) {
    u32 val = pci_read32(bus, dev, func, offset & 0xFC);
    return (u16)(val >> ((offset & 2) * 8));
}

u8 pci_read8(u8 bus, u8 dev, u8 func, u8 offset) {
    u32 val = pci_read32(bus, dev, func, offset & 0xFC);
    return (u8)(val >> ((offset & 3) * 8));
}

/* ── Config-space writes ─────────────────────────────────────────────────── */

void pci_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 val) {
    outl(PCI_CONFIG_ADDR, pci_addr(bus, dev, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 val) {
    u32 dword = pci_read32(bus, dev, func, offset & 0xFC);
    u32 shift = (offset & 2) * 8;
    dword = (dword & ~(0xFFFFU << shift)) | ((u32)val << shift);
    pci_write32(bus, dev, func, offset & 0xFC, dword);
}

/* ── Fill a pci_device_t from config-space ───────────────────────────────── */

static void pci_fill_device(u8 bus, u8 dev, u8 func, pci_device_t *out) {
    out->bus       = bus;
    out->dev       = dev;
    out->func      = func;
    out->vendor_id = pci_read16(bus, dev, func, PCI_VENDOR_ID);
    out->device_id = pci_read16(bus, dev, func, PCI_DEVICE_ID);

    u32 class_dword = pci_read32(bus, dev, func, PCI_REVISION_ID);
    out->revision   = (u8)(class_dword);
    out->prog_if    = (u8)(class_dword >> 8);
    out->subclass   = (u8)(class_dword >> 16);
    out->class_code = (u8)(class_dword >> 24);

    out->irq_line = pci_read8(bus, dev, func, PCI_INTERRUPT_LINE);

    for (u8 i = 0; i < 6; i++)
        out->bar[i] = pci_read32(bus, dev, func, PCI_BAR0 + i * 4);
}

/* ── Enumerate all PCI functions ─────────────────────────────────────────── */

void pci_enumerate(pci_enum_cb_t cb, void *arg) {
    for (u16 bus = 0; bus < 256; bus++) {
        for (u8 dev = 0; dev < 32; dev++) {
            /* Check function 0 first */
            u16 vendor = pci_read16((u8)bus, dev, 0, PCI_VENDOR_ID);
            if (vendor == PCI_VENDOR_NONE)
                continue;

            u8 hdr_type = pci_read8((u8)bus, dev, 0, PCI_HEADER_TYPE);
            u8 max_func = (hdr_type & 0x80) ? 8 : 1; /* multi-function? */

            for (u8 func = 0; func < max_func; func++) {
                vendor = pci_read16((u8)bus, dev, func, PCI_VENDOR_ID);
                if (vendor == PCI_VENDOR_NONE)
                    continue;

                pci_device_t info;
                pci_fill_device((u8)bus, dev, func, &info);
                cb(&info, arg);
            }
        }
    }
}

/* ── pci_find_by_class ───────────────────────────────────────────────────── */

typedef struct { u8 cls; u8 sub; pci_device_t *out; bool found; } find_ctx_t;

static void find_cb(const pci_device_t *d, void *arg) {
    find_ctx_t *ctx = (find_ctx_t *)arg;
    if (!ctx->found && d->class_code == ctx->cls && d->subclass == ctx->sub) {
        *ctx->out = *d;
        ctx->found = true;
    }
}

bool pci_find_by_class(u8 class_code, u8 subclass, pci_device_t *out) {
    find_ctx_t ctx = { class_code, subclass, out, false };
    pci_enumerate(find_cb, &ctx);
    return ctx.found;
}
