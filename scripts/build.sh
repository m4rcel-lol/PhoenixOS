#!/bin/bash
# build.sh — PhoenixOS top-level build script

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ── Colors ────────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

info()  { echo -e "${CYAN}[build]${NC} $*"; }
ok()    { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn()  { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
error() { echo -e "${RED}[ERROR ]${NC} $*"; exit 1; }

# ── Check cross-compiler ──────────────────────────────────────────────────────
CROSS_CC="${CROSS_PREFIX:-x86_64-elf-}gcc"
if ! command -v "$CROSS_CC" &>/dev/null; then
    error "Cross-compiler not found: $CROSS_CC\nRun scripts/setup-cross.sh first."
fi
ok "Cross-compiler found: $(which $CROSS_CC)"

# Check NASM
if ! command -v nasm &>/dev/null; then
    error "NASM not found. Install with: sudo apt install nasm"
fi
ok "NASM found: $(nasm --version | head -1)"

# ── Build kernel ──────────────────────────────────────────────────────────────
info "Building EmberKernel..."
make -C "$ROOT" kernel -j"$(nproc)" 2>&1 | \
    while IFS= read -r line; do echo "  $line"; done
ok "EmberKernel built -> kernel/ember.elf"

# ── Build userspace ───────────────────────────────────────────────────────────
info "Building userspace..."
make -C "$ROOT" userspace -j"$(nproc)" 2>&1 | \
    while IFS= read -r line; do echo "  $line"; done
ok "Userspace built"

# ── Build Rust services ───────────────────────────────────────────────────────
if command -v cargo &>/dev/null; then
    info "Building Rust services..."
    (cd "$ROOT/userspace/services" && cargo build --release 2>&1 | \
        while IFS= read -r line; do echo "  $line"; done)
    ok "Rust services built"
else
    warn "cargo not found — skipping Rust service build"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}PhoenixOS build complete!${NC}"
echo "  Kernel:    $ROOT/kernel/ember.elf"
echo "  Userspace: $ROOT/build/userspace/"
echo ""
echo "Next steps:"
echo "  scripts/make-image.sh   Create disk/ISO image"
echo "  scripts/run-qemu.sh     Boot in QEMU"
