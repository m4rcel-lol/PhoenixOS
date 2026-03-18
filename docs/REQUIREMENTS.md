# PhoenixOS — Minimum Hardware Requirements

This document lists the minimum and recommended hardware specifications to run PhoenixOS.

---

## Minimum Requirements

| Component | Minimum | Notes |
|-----------|---------|-------|
| **CPU** | x86_64 (64-bit) @ 500 MHz | Must support Long Mode (CPUID LM bit). 32-bit CPUs are **not** supported. |
| **RAM** | 64 MB | The kernel and early userspace fit comfortably in 64 MB. Less than 32 MB will cause the physical memory manager to fail allocation. |
| **Disk / Storage** | 256 MB | Minimum space for kernel, userspace, and ext2 root filesystem. |
| **Display** | BIOS/UEFI VGA (640×480, 32 bpp) | The framebuffer driver requires a 32-bit-per-pixel linear framebuffer provided via the Multiboot2 tag. 16 bpp modes are not supported. |
| **Bootloader** | GRUB2 (Multiboot2) | GRUB2 loads EmberKernel using the Multiboot2 protocol. UEFI and BIOS boot are both supported via GRUB. |
| **Firmware** | BIOS (legacy) or UEFI (64-bit) | Legacy BIOS and 64-bit UEFI with GRUB2 EFI binary are both supported. 32-bit UEFI is **not** supported. |

---

## Recommended Requirements

| Component | Recommended | Notes |
|-----------|-------------|-------|
| **CPU** | x86_64 @ 1 GHz or faster | Multi-core CPUs provide better responsiveness. SMP support is planned for Phase 6. |
| **RAM** | 256 MB | Comfortable for kernel + Kindle init + AshDE desktop session. |
| **Disk / Storage** | 1 GB | Room for the base system, packages, and user data. |
| **Display** | 1024×768 @ 32 bpp or higher | AshDE is designed for 1024×768 and above. |

---

## VirtualBox Setup

If you are running PhoenixOS in VirtualBox and GRUB appears but the screen goes blank afterwards:

1. **Enable "Enable PAE/NX"** in *Settings → System → Processor*. EmberKernel uses PAE for the physical address extension required before Long Mode is activated.
2. **Set display to VBoxVGA** (not VMSVGA or VBoxSVGA). PhoenixOS uses a linear VESA framebuffer; newer VirtualBox display adapters may not expose one in a way that GRUB's Multiboot2 tag can populate correctly.
3. **Allocate at least 64 MB of RAM** (*Settings → System → Motherboard → Base Memory*).
4. **Set "Graphics Controller" to VBoxVGA** and ensure 3D acceleration is **disabled** (*Settings → Display → Screen*).
5. **Use a fixed-size VDI disk** of at least 256 MB or boot from the ISO with the CD-ROM controller set to IDE.

### Recommended VirtualBox Settings

| VirtualBox Setting | Value |
|--------------------|-------|
| Type / Version | Other / Other/Unknown (64-bit) |
| Base Memory | 256 MB |
| CPUs | 1 (SMP not yet supported) |
| Graphics Controller | VBoxVGA |
| Video Memory | 16 MB |
| Storage Controller | IDE (for ISO) |
| 3D Acceleration | Disabled |

---

## QEMU Reference Command

The recommended way to run PhoenixOS during development is QEMU:

```bash
qemu-system-x86_64 \
    -m 256M \
    -cdrom phoenix.iso \
    -boot d \
    -serial stdio \
    -vga std \
    -no-reboot -no-shutdown
```

Or using the provided script:

```bash
bash scripts/run-qemu.sh --memory=256M
```

---

## Notes on 32-bit CPUs

EmberKernel **requires a 64-bit (x86_64) CPU**. The `boot.asm` entry point checks
for Long Mode support via `CPUID` and halts with an `'L'` error displayed at the
top-left of the screen if a 64-bit CPU is not detected. There are no plans to
support 32-bit (i386/i486/i586/i686) CPUs.
