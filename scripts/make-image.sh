#!/bin/bash
# make-image.sh — Create PhoenixOS bootable disk image and ISO

set -eo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
ISO_DIR="$BUILD/isoroot"
KERNEL="$ROOT/kernel/ember.elf"
ISO_OUT="$ROOT/phoenix.iso"
DISK_OUT="$ROOT/phoenix.img"
DISK_SIZE_MB=512

# ── Helpers ───────────────────────────────────────────────────────────────────
info() { echo "[image] $*"; }
check_tool() { command -v "$1" &>/dev/null || { echo "Missing tool: $1"; exit 1; }; }

# ── Check prerequisites ───────────────────────────────────────────────────────
check_tool grub-mkrescue
check_tool mkfs.ext2

if [[ ! -f "$KERNEL" ]]; then
    echo "Kernel not found: $KERNEL"
    echo "Run make kernel first."
    exit 1
fi

# ── Build ISO image ───────────────────────────────────────────────────────────
info "Building ISO image..."

rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"

# Copy kernel
cp "$KERNEL" "$ISO_DIR/boot/ember.elf"

# Copy GRUB config
cp "$ROOT/boot/grub.cfg" "$ISO_DIR/boot/grub/grub.cfg"

# Copy userspace files into initrd-like location
mkdir -p "$ISO_DIR/init" "$ISO_DIR/bin" "$ISO_DIR/lib"
if [[ -d "$BUILD/userspace" ]]; then
    cp -r "$BUILD/userspace/"* "$ISO_DIR/bin/" 2>/dev/null || true
fi

# Copy installer binary if present
for installer_path in \
        "$ROOT/userspace/install/phoenix-install" \
        "$BUILD/userspace/phoenix-install"; do
    if [[ -f "$installer_path" ]]; then
        cp "$installer_path" "$ISO_DIR/bin/phoenix-install"
        info "Installer copied: $installer_path"
        break
    fi
done

# Build the ISO
info "Running grub-mkrescue..."
grub-mkrescue -o "$ISO_OUT" "$ISO_DIR" -- \
    -volid "PHOENIXOS" \
    -joliet on

info "ISO created: $ISO_OUT ($(du -sh "$ISO_OUT" | cut -f1))"

# Verify the ISO is not empty and contains the kernel
ISO_SIZE=$(stat -c%s "$ISO_OUT" 2>/dev/null || stat -f%z "$ISO_OUT")
if [[ "$ISO_SIZE" -lt 1048576 ]]; then
    echo "ERROR: ISO is suspiciously small (${ISO_SIZE} bytes). Build may have failed."
    exit 1
fi
if command -v isoinfo &>/dev/null; then
    if ! isoinfo -i "$ISO_OUT" -R -l 2>/dev/null | grep -q ember.elf; then
        echo "ERROR: ember.elf not found inside the ISO. Build may have failed."
        exit 1
    fi
    info "Kernel verified inside ISO."
fi
info "ISO verification passed (size: $(du -sh "$ISO_OUT" | cut -f1))"

# ── Build raw disk image ──────────────────────────────────────────────────────
info "Building raw disk image (${DISK_SIZE_MB}MB)..."

dd if=/dev/zero of="$DISK_OUT" bs=1M count="$DISK_SIZE_MB" status=progress

# Create partition table with a single ext2 root partition
# Using loop device for a fully bootable setup
if command -v losetup &>/dev/null && [[ -w /dev/loop0 || $(id -u) -eq 0 ]]; then
    LOOP=$(losetup --find --show "$DISK_OUT")
    info "Loop device: $LOOP"

    # Create partition table
    parted -s "$LOOP" mklabel msdos
    parted -s "$LOOP" mkpart primary ext2 1MiB 100%
    parted -s "$LOOP" set 1 boot on

    # Format
    LOOP_PART="${LOOP}p1"
    if [[ -b "$LOOP_PART" ]]; then
        mkfs.ext2 "$LOOP_PART"

        # Mount and populate
        MNT=$(mktemp -d)
        mount "$LOOP_PART" "$MNT"

        mkdir -p "$MNT/boot/grub" "$MNT/bin" "$MNT/etc" "$MNT/var/log"
        cp "$KERNEL" "$MNT/boot/ember.elf"
        cp "$ROOT/boot/grub.cfg" "$MNT/boot/grub/grub.cfg"

        # Install GRUB to disk
        grub-install --target=i386-pc --root-directory="$MNT" "$LOOP" 2>/dev/null || true

        umount "$MNT"
        rmdir "$MNT"
    fi

    losetup -d "$LOOP"
    info "Raw disk image created: $DISK_OUT"
else
    info "Skipping raw disk setup (need root/loop device access)"
fi

info ""
info "Done! Images:"
info "  ISO:  $ISO_OUT"
info "  Disk: $DISK_OUT"
info ""
info "Boot with: scripts/run-qemu.sh"
