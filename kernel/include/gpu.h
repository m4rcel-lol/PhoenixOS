#ifndef KERNEL_GPU_H
#define KERNEL_GPU_H

#include "kernel.h"

/* ── GPU vendor IDs ──────────────────────────────────────────────────────── */

#define GPU_PCI_VENDOR_INTEL   0x8086
#define GPU_PCI_VENDOR_AMD     0x1002
#define GPU_PCI_VENDOR_NVIDIA  0x10DE
#define GPU_PCI_VENDOR_VMWARE  0x15AD
#define GPU_PCI_VENDOR_BOCHS   0x1234   /* QEMU/Bochs VBE device */

/* ── GPU vendor enum ─────────────────────────────────────────────────────── */

typedef enum {
    GPU_VENDOR_UNKNOWN = 0,
    GPU_VENDOR_INTEL,
    GPU_VENDOR_AMD,
    GPU_VENDOR_NVIDIA,
    GPU_VENDOR_VMWARE,
    GPU_VENDOR_BOCHS,
} gpu_vendor_t;

/* ── GPU info ────────────────────────────────────────────────────────────── */

typedef struct {
    gpu_vendor_t vendor;
    u16          pci_vendor_id;
    u16          pci_device_id;
    u32          vram_mb;       /* VRAM size in MiB (0 if unknown) */
    bool         accel_avail;   /* hardware acceleration available */
} gpu_info_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

void gpu_init(void);
bool gpu_get_info(gpu_info_t *info);

#endif /* KERNEL_GPU_H */
