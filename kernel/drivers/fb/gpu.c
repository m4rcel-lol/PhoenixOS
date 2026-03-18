#include "../../include/kernel.h"
#include "../../include/gpu.h"
#include "../../include/pci.h"

/* ── Internal state ──────────────────────────────────────────────────────── */

static gpu_info_t gpu_info;
static bool       gpu_detected = false;

/* ── Vendor name helper ──────────────────────────────────────────────────── */

static const char *vendor_name(gpu_vendor_t v) {
    switch (v) {
        case GPU_VENDOR_INTEL:  return "Intel";
        case GPU_VENDOR_AMD:    return "AMD";
        case GPU_VENDOR_NVIDIA: return "NVIDIA";
        case GPU_VENDOR_VMWARE: return "VMware";
        case GPU_VENDOR_BOCHS:  return "QEMU/Bochs VBE";
        default:                return "Unknown";
    }
}

/* ── Map a PCI vendor ID to our gpu_vendor_t ─────────────────────────────── */

static gpu_vendor_t classify_vendor(u16 vid) {
    switch (vid) {
        case GPU_PCI_VENDOR_INTEL:  return GPU_VENDOR_INTEL;
        case GPU_PCI_VENDOR_AMD:    return GPU_VENDOR_AMD;
        case GPU_PCI_VENDOR_NVIDIA: return GPU_VENDOR_NVIDIA;
        case GPU_PCI_VENDOR_VMWARE: return GPU_VENDOR_VMWARE;
        case GPU_PCI_VENDOR_BOCHS:  return GPU_VENDOR_BOCHS;
        default:                    return GPU_VENDOR_UNKNOWN;
    }
}

/* ── PCI enumeration callback ────────────────────────────────────────────── */

static void gpu_pci_cb(const pci_device_t *d, void *arg) {
    (void)arg;
    if (gpu_detected) return;   /* already found one */

    /* Accept VGA-compatible, XGA, 3D or "other display" subclasses */
    if (d->class_code != PCI_CLASS_DISPLAY)
        return;

    gpu_info.pci_vendor_id = d->vendor_id;
    gpu_info.pci_device_id = d->device_id;
    gpu_info.vendor        = classify_vendor(d->vendor_id);
    gpu_info.vram_mb       = 0;      /* runtime VRAM probing not implemented */
    gpu_info.accel_avail   = false;  /* no 2D/3D acceleration yet */

    gpu_detected = true;
}

/* ── gpu_init ────────────────────────────────────────────────────────────── */

void gpu_init(void) {
    gpu_detected = false;
    pci_enumerate(gpu_pci_cb, NULL);

    if (gpu_detected) {
        printk("[gpu ] Detected %s graphics card (PCI %04x:%04x)\n",
               vendor_name(gpu_info.vendor),
               gpu_info.pci_vendor_id,
               gpu_info.pci_device_id);
    } else {
        printk("[gpu ] No PCI graphics card detected; "
               "using bootloader framebuffer\n");
    }
}

/* ── gpu_get_info ────────────────────────────────────────────────────────── */

bool gpu_get_info(gpu_info_t *info) {
    if (!gpu_detected) return false;
    *info = gpu_info;
    return true;
}
