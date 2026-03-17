#!/bin/bash
# setup-cross.sh — Build x86_64-elf cross-compiler for PhoenixOS development

set -e

# ── Configuration ─────────────────────────────────────────────────────────────
PREFIX="${PREFIX:-$HOME/opt/cross}"
BINUTILS_VERSION="2.41"
GCC_VERSION="13.2.0"
TARGET="x86_64-elf"
JOBS="${JOBS:-$(nproc)}"

BINUTILS_URL="https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz"
GCC_URL="https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz"

BUILD_DIR="$(mktemp -d /tmp/cross-build.XXXX)"

info()  { echo -e "\033[0;36m[cross]\033[0m $*"; }
ok()    { echo -e "\033[0;32m[  OK  ]\033[0m $*"; }
error() { echo -e "\033[0;31m[ERROR ]\033[0m $*"; exit 1; }

# ── Check host tools ──────────────────────────────────────────────────────────
for tool in gcc g++ make wget tar flex bison; do
    command -v "$tool" &>/dev/null || error "Missing host tool: $tool (install build-essential)"
done
ok "Host tools verified"

export PATH="$PREFIX/bin:$PATH"

mkdir -p "$PREFIX"
cd "$BUILD_DIR"

# ── Download sources ──────────────────────────────────────────────────────────
info "Downloading binutils $BINUTILS_VERSION..."
wget -q --show-progress "$BINUTILS_URL" -O binutils.tar.xz
tar -xf binutils.tar.xz
ok "binutils extracted"

info "Downloading GCC $GCC_VERSION..."
wget -q --show-progress "$GCC_URL" -O gcc.tar.xz
tar -xf gcc.tar.xz
ok "GCC extracted"

# Download GCC prerequisites
cd "gcc-${GCC_VERSION}"
./contrib/download_prerequisites
cd ..

# ── Build binutils ────────────────────────────────────────────────────────────
info "Building binutils (this may take a few minutes)..."
mkdir -p build-binutils
cd build-binutils
"$BUILD_DIR/binutils-${BINUTILS_VERSION}/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror \
    2>&1 | tail -3

make -j"$JOBS" 2>&1 | tail -5
make install 2>&1 | tail -3
cd ..
ok "binutils installed to $PREFIX"

# ── Build GCC ─────────────────────────────────────────────────────────────────
info "Building GCC $GCC_VERSION (this takes 10–30 minutes)..."
mkdir -p build-gcc
cd build-gcc
"$BUILD_DIR/gcc-${GCC_VERSION}/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers \
    --disable-hosted-libstdcxx \
    2>&1 | tail -3

make -j"$JOBS" all-gcc 2>&1 | tail -5
make -j"$JOBS" all-target-libgcc 2>&1 | tail -5
make install-gcc 2>&1 | tail -3
make install-target-libgcc 2>&1 | tail -3
cd ..
ok "GCC installed to $PREFIX"

# ── Install Rust cross target ─────────────────────────────────────────────────
if command -v rustup &>/dev/null; then
    info "Installing Rust target x86_64-unknown-none..."
    rustup target add x86_64-unknown-none
    ok "Rust target installed"
else
    echo "[cross] Note: rustup not found — install from https://rustup.rs for Rust support"
fi

# ── Cleanup ───────────────────────────────────────────────────────────────────
cd /
rm -rf "$BUILD_DIR"

# ── Final instructions ────────────────────────────────────────────────────────
echo ""
ok "Cross-compiler setup complete!"
echo ""
echo "Add to your shell profile (~/.bashrc or ~/.zshrc):"
echo "  export PATH=\"$PREFIX/bin:\$PATH\""
echo "  export CROSS_PREFIX=\"x86_64-elf-\""
echo ""
echo "Verify with:"
echo "  x86_64-elf-gcc --version"
echo "  x86_64-elf-ld --version"
