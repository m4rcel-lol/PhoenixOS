#!/bin/bash
# run-qemu.sh — Boot PhoenixOS in QEMU

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# ── Defaults ──────────────────────────────────────────────────────────────────
MEMORY="${MEMORY:-256M}"
CPUS="${CPUS:-2}"
IMAGE="${IMAGE:-$ROOT/phoenix.iso}"
USE_KVM=0
DEBUG_MODE=0
WAIT_FOR_GDB=0

# ── Parse arguments ───────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --kvm)       USE_KVM=1       ;;
        --debug)     DEBUG_MODE=1    ;;
        --wait-gdb)  WAIT_FOR_GDB=1  ;;
        --memory=*)  MEMORY="${1#*=}" ;;
        --image=*)   IMAGE="${1#*=}" ;;
        -h|--help)
            echo "Usage: $0 [--kvm] [--debug] [--wait-gdb] [--memory=NM] [--image=path]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

# ── Check prerequisites ───────────────────────────────────────────────────────
if ! command -v qemu-system-x86_64 &>/dev/null; then
    echo "Error: qemu-system-x86_64 not found."
    echo "Install: sudo apt install qemu-system-x86"
    exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
    echo "Error: image not found: $IMAGE"
    echo "Run scripts/make-image.sh first."
    exit 1
fi

# ── Build QEMU command ────────────────────────────────────────────────────────
QEMU_ARGS=(
    -m "$MEMORY"
    -smp "$CPUS"
    -cdrom "$IMAGE"
    -boot d
    -serial stdio
    -vga std
    -no-reboot
    -no-shutdown
    -name "PhoenixOS"
)

# KVM acceleration
if [[ $USE_KVM -eq 1 ]]; then
    if [[ -r /dev/kvm ]]; then
        QEMU_ARGS+=(-enable-kvm -cpu host)
        echo "[qemu] KVM acceleration enabled"
    else
        echo "[qemu] Warning: KVM not available, running without acceleration"
    fi
fi

# Debug mode (GDB stub on port 1234)
if [[ $DEBUG_MODE -eq 1 ]]; then
    QEMU_ARGS+=(-s)
    if [[ $WAIT_FOR_GDB -eq 1 ]]; then
        QEMU_ARGS+=(-S)
        echo "[qemu] Waiting for GDB on :1234 (run scripts/debug.sh in another terminal)"
    else
        echo "[qemu] GDB stub on port 1234"
    fi
fi

echo "[qemu] Starting PhoenixOS (memory=$MEMORY, cpus=$CPUS)"
echo "[qemu] Image: $IMAGE"
echo ""

exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
