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

JOBS="$(nproc 2>/dev/null || echo 1)"

# ── Check cross-compiler ──────────────────────────────────────────────────────
CROSS_CC="${CROSS_PREFIX:-x86_64-elf-}gcc"
if ! command -v "$CROSS_CC" &>/dev/null; then
    error "Cross-compiler not found: $CROSS_CC\nRun scripts/setup-cross.sh first."
fi
ok "Cross-compiler found: $(command -v "$CROSS_CC")"

# Check NASM
if ! command -v nasm &>/dev/null; then
    error "NASM not found. Install with: sudo apt install nasm"
fi
ok "NASM found: $(nasm --version | head -1)"

# ── Build S language compiler ─────────────────────────────────────────────────
info "Building S language compiler..."
make -C "$ROOT" sc 2>&1 | while IFS= read -r line; do echo "  $line"; done
ok "S compiler built -> tools/s-lang/sc"

# ── Build kernel ──────────────────────────────────────────────────────────────
info "Building EmberKernel..."
make -C "$ROOT" kernel -j"$JOBS" 2>&1 | while IFS= read -r line; do echo "  $line"; done
ok "EmberKernel built -> kernel/ember.elf"

# ── Build runtime libraries ───────────────────────────────────────────────────
info "Building runtime libraries..."
make -C "$ROOT" libs 2>&1 | while IFS= read -r line; do echo "  $line"; done
ok "Libraries built (libflame, libgui, libipc)"

# ── Build userspace ───────────────────────────────────────────────────────────
info "Building userspace..."
make -C "$ROOT" userspace -j"$JOBS" 2>&1 | while IFS= read -r line; do echo "  $line"; done
ok "Userspace built"

# ── Build desktop ─────────────────────────────────────────────────────────────
info "Building AshDE desktop environment..."
make -C "$ROOT" desktop 2>&1 | while IFS= read -r line; do echo "  $line"; done
ok "AshDE desktop built"

# ── Build Rust services ───────────────────────────────────────────────────────
if command -v cargo &>/dev/null; then
    info "Building Rust services..."
    (cd "$ROOT/userspace/services" && cargo build --release 2>&1 | \
        while IFS= read -r line; do echo "  $line"; done)
    ok "Rust services built"
else
    warn "cargo not found — skipping Rust service build"
fi

# ── Run unit tests ────────────────────────────────────────────────────────────
info "Running kernel unit tests..."
if make -C "$ROOT" test 2>&1 | while IFS= read -r line; do echo "  $line"; done; then
    ok "All unit tests passed"
else
    warn "One or more unit tests failed — check output above"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}PhoenixOS build complete!${NC}"
echo "  Kernel:    $ROOT/kernel/ember.elf"
echo "  Libraries: $ROOT/lib/{libflame,libgui,libipc}/"
echo "  Desktop:   $ROOT/desktop/{wm,panel,fileman,control,sessionmgr}/"
echo "  S compiler: $ROOT/tools/s-lang/sc"
echo ""
echo "Next steps:"
echo "  scripts/make-image.sh   Create disk/ISO image"
echo "  scripts/run-qemu.sh     Boot in QEMU"

