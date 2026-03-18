#!/bin/bash
# make-vm-image.sh — Create a VM-compatible disk image for VirtualBox and VMware
#
# Produces:
#   phoenix.vmdk  — Virtual hard-drive image (importable in VirtualBox / VMware)
#
# Requirements (install via apt):
#   grub-pc-bin grub-common e2fsprogs parted qemu-utils

set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL="$ROOT/kernel/ember.elf"
GRUB_CFG="$ROOT/boot/grub.cfg"
DISK_OUT="$ROOT/phoenix.img"
VMDK_OUT="$ROOT/phoenix.vmdk"
DISK_SIZE_MB=256

# ── Helpers ───────────────────────────────────────────────────────────────────
info()  { echo "[vm-image] $*"; }
error() { echo "[vm-image] ERROR: $*" >&2; exit 1; }
check_tool() { command -v "$1" &>/dev/null || error "Missing tool: $1. Install it first."; }

# ── Prerequisites ─────────────────────────────────────────────────────────────
check_tool grub-install
check_tool mkfs.ext2
check_tool parted
check_tool qemu-img

[[ -f "$KERNEL" ]] || error "Kernel not found: $KERNEL — run 'make kernel' first."
[[ -f "$GRUB_CFG" ]] || error "GRUB config not found: $GRUB_CFG"

if [[ "$(id -u)" -ne 0 ]]; then
    error "This script must be run as root (sudo) to set up loop devices."
fi

# ── Create raw disk image ─────────────────────────────────────────────────────
info "Creating ${DISK_SIZE_MB} MB raw disk image: $DISK_OUT"
dd if=/dev/zero of="$DISK_OUT" bs=1M count="$DISK_SIZE_MB" status=progress

# ── Partition ─────────────────────────────────────────────────────────────────
info "Creating MBR partition table..."
parted -s "$DISK_OUT" mklabel msdos
parted -s "$DISK_OUT" mkpart primary ext2 1MiB 100%
parted -s "$DISK_OUT" set 1 boot on

# ── Loop device ───────────────────────────────────────────────────────────────
info "Setting up loop device..."
LOOP=$(losetup --find --show --partscan "$DISK_OUT")
info "Loop device: $LOOP"

MNT=""
cleanup() {
    [[ -n "$MNT" ]] && { umount "$MNT" 2>/dev/null || true; rmdir "$MNT" 2>/dev/null || true; }
    [[ -n "$LOOP" ]] && losetup -d "$LOOP" 2>/dev/null || true
}
trap cleanup EXIT

# Wait for the partition device node to appear (retry up to 10 seconds)
LOOP_PART="${LOOP}p1"
for i in $(seq 1 10); do
    [[ -b "$LOOP_PART" ]] && break
    partprobe "$LOOP" 2>/dev/null || true
    sleep 1
done
[[ -b "$LOOP_PART" ]] || error "Partition device $LOOP_PART did not appear after 10 seconds."

# ── Format ────────────────────────────────────────────────────────────────────
info "Formatting partition as ext2..."
mkfs.ext2 -L "PHOENIXOS" "$LOOP_PART"

# ── Mount and populate ────────────────────────────────────────────────────────
MNT=$(mktemp -d /tmp/phoenix-mnt.XXXXXX)
mount "$LOOP_PART" "$MNT"

info "Copying kernel and GRUB config..."
mkdir -p "$MNT/boot/grub"
cp "$KERNEL"   "$MNT/boot/ember.elf"
cp "$GRUB_CFG" "$MNT/boot/grub/grub.cfg"

# ── Install GRUB ──────────────────────────────────────────────────────────────
info "Installing GRUB bootloader..."
grub-install \
    --target=i386-pc \
    --root-directory="$MNT" \
    --no-floppy \
    --modules="normal part_msdos ext2 multiboot2 multiboot" \
    "$LOOP"

umount "$MNT"
MNT=""
losetup -d "$LOOP"
LOOP=""
trap - EXIT

# ── Convert to VMDK ───────────────────────────────────────────────────────────
info "Converting raw image to VMDK format..."
qemu-img convert -f raw -O vmdk "$DISK_OUT" "$VMDK_OUT"

info ""
info "Done! VM image:"
info "  Raw disk: $DISK_OUT  ($(du -sh "$DISK_OUT" | cut -f1))"
info "  VMDK:     $VMDK_OUT  ($(du -sh "$VMDK_OUT" | cut -f1))"
info ""
info "Usage in Oracle VirtualBox:"
info "  1. Create new VM — Type: Other, Version: Other/Unknown (64-bit)"
info "  2. At 'Hard Disk' step choose 'Use an existing virtual hard disk file'"
info "  3. Select: $VMDK_OUT"
info ""
info "Usage in VMware Workstation / Player:"
info "  1. Create a Custom VM — select 'I will install the OS later'"
info "  2. Under Hardware > Hard Disk, remove the default disk"
info "  3. Add Hardware > Hard Disk > Use an existing virtual disk"
info "  4. Select: $VMDK_OUT"
