#!/bin/bash
# clean.sh — Remove PhoenixOS build artifacts

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "[clean] Removing build artifacts..."

# Kernel objects and ELF
find "$ROOT/kernel" -name "*.o" -delete
find "$ROOT/kernel" -name "*.d" -delete
rm -f "$ROOT/kernel/ember.elf"

# Userspace binaries
find "$ROOT/userspace" -name "*.o" -delete
find "$ROOT/userspace/coreutils" -maxdepth 1 -executable -type f -delete 2>/dev/null || true

# Library objects
find "$ROOT/lib" -name "*.o" -delete
find "$ROOT/lib" -name "*.a" -delete

# Desktop binaries
find "$ROOT/desktop" -name "*.o" -delete
find "$ROOT/desktop" -maxdepth 2 -executable -type f -delete 2>/dev/null || true

# Rust targets
if [[ -d "$ROOT/userspace/services/target" ]]; then
    rm -rf "$ROOT/userspace/services/target"
fi

# Build output
rm -rf "$ROOT/build/isoroot"
rm -rf "$ROOT/build/userspace"
rm -f  "$ROOT/build/debug.gdb"

# Images
rm -f "$ROOT/phoenix.iso"
rm -f "$ROOT/phoenix.img"

echo "[clean] Done."
