#!/bin/bash
# debug.sh — Debug PhoenixOS with QEMU + GDB

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
KERNEL_ELF="$ROOT/kernel/ember.elf"
GDB_SCRIPT="$ROOT/build/debug.gdb"
PORT=1234

# ── Generate GDB init script ──────────────────────────────────────────────────
cat > "$GDB_SCRIPT" <<'EOF'
# PhoenixOS GDB debug script

set architecture i386:x86-64
target remote :1234

# Load kernel symbols
file build/kernel/ember.elf

# Useful breakpoints
# break kernel_start
# break panic
# break schedule

echo \nEmberKernel GDB session ready.\n
echo Type 'continue' to start, or set breakpoints first.\n
echo \nUseful commands:\n
echo   break kernel_start    - break at kernel entry\n
echo   break panic           - break on kernel panic\n
echo   info registers        - show CPU registers\n
echo   x/10i $rip           - disassemble at current instruction\n
echo   backtrace             - show call stack\n
echo \n
EOF

# ── Check kernel ELF ─────────────────────────────────────────────────────────
if [[ ! -f "$KERNEL_ELF" ]]; then
    echo "Warning: kernel/ember.elf not found — build first with 'make kernel'"
fi

# ── Start QEMU in background ─────────────────────────────────────────────────
QEMU_PID=""
cleanup() {
    if [[ -n "$QEMU_PID" ]]; then
        echo "[debug] Killing QEMU (pid $QEMU_PID)"
        kill "$QEMU_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "[debug] Starting QEMU with GDB stub on port $PORT..."
"$ROOT/scripts/run-qemu.sh" --debug --wait-gdb &
QEMU_PID=$!
sleep 1

# ── Start GDB ─────────────────────────────────────────────────────────────────
echo "[debug] Starting GDB..."
GDB_CMD="gdb"

# Prefer gdb-multiarch or x86_64-elf-gdb if available
for g in gdb-multiarch x86_64-elf-gdb gdb; do
    if command -v "$g" &>/dev/null; then
        GDB_CMD="$g"
        break
    fi
done

echo "[debug] Using: $GDB_CMD"
"$GDB_CMD" -q \
    -ex "set architecture i386:x86-64" \
    -ex "target remote :$PORT" \
    $(test -f "$KERNEL_ELF" && echo "-ex 'file $KERNEL_ELF'") \
    -ex "echo \nEmberKernel debug ready. Type 'continue' to boot.\n" \
    --args "$KERNEL_ELF"
