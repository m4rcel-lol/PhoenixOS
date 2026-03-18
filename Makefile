ARCH        := x86_64
CROSS_PREFIX := x86_64-elf-
CC          := $(CROSS_PREFIX)gcc
AS          := nasm
LD          := $(CROSS_PREFIX)ld
OBJCOPY     := $(CROSS_PREFIX)objcopy
AR          := $(CROSS_PREFIX)ar

ISO         := phoenix.iso
IMAGE       := phoenix.img
KERNEL      := kernel/ember.elf
GRUB_CFG    := boot/grub.cfg

QEMU        := qemu-system-x86_64
QEMU_FLAGS  := -m 256M -serial stdio -vga std -no-reboot -no-shutdown
QEMU_CDROM  := -cdrom $(ISO) -boot d

# Number of parallel jobs (auto-detect CPU count)
NPROC := $(shell nproc 2>/dev/null || echo 1)

.PHONY: all kernel libs userspace desktop services iso image run run-kvm \
        debug test clean help

all: kernel libs userspace desktop iso

# ── Kernel ────────────────────────────────────────────────────────────────────
kernel:
	$(MAKE) -C kernel -j$(NPROC)

# ── Runtime libraries (must be built before desktop) ─────────────────────────
libs:
	$(MAKE) -C lib/libflame
	$(MAKE) -C lib/libgui
	$(MAKE) -C lib/libipc

# ── Userspace programs ────────────────────────────────────────────────────────
userspace:
	$(MAKE) -C userspace/coreutils
	$(MAKE) -C userspace/init
	$(MAKE) -C userspace/shell
	$(MAKE) -C userspace/login
	$(MAKE) -C userspace/install

# ── Rust services (optional — skipped if cargo is absent) ────────────────────
services:
	@if command -v cargo >/dev/null 2>&1; then \
	    echo "[cargo] Building Rust services..."; \
	    cd userspace/services && cargo build --release; \
	else \
	    echo "[SKIP] cargo not found — skipping Rust services"; \
	fi

# ── AshDE desktop environment (depends on libs) ──────────────────────────────
desktop: libs
	$(MAKE) -C desktop/wm
	$(MAKE) -C desktop/panel
	$(MAKE) -C desktop/fileman
	$(MAKE) -C desktop/control
	$(MAKE) -C desktop/sessionmgr

# ── ISO image ────────────────────────────────────────────────────────────────
iso: kernel userspace desktop
	@echo "[ISO] Building $(ISO)..."
	@bash scripts/make-image.sh

# ── Raw disk image ────────────────────────────────────────────────────────────
image: iso
	@echo "[IMG] Building disk image $(IMAGE)..."
	@dd if=/dev/zero of=$(IMAGE) bs=1M count=512 status=none
	@parted -s $(IMAGE) mklabel msdos mkpart primary 2048s 100%
	@mkfs.ext2 -F -b 4096 $(IMAGE)

# ── QEMU ─────────────────────────────────────────────────────────────────────
run: iso
	$(QEMU) $(QEMU_FLAGS) $(QEMU_CDROM)

run-kvm: iso
	$(QEMU) $(QEMU_FLAGS) $(QEMU_CDROM) -enable-kvm

# ── GDB + QEMU debug session ─────────────────────────────────────────────────
debug: iso
	@bash scripts/debug.sh

# ── Unit tests ────────────────────────────────────────────────────────────────
test:
	$(MAKE) -C tests/kernel run

# ── S language compiler ───────────────────────────────────────────────────────
sc:
	$(MAKE) -C tools/s-lang

# ── Clean all artifacts ───────────────────────────────────────────────────────
clean:
	-$(MAKE) -C kernel clean
	-$(MAKE) -C lib/libflame clean
	-$(MAKE) -C lib/libgui clean
	-$(MAKE) -C lib/libipc clean
	-$(MAKE) -C userspace/coreutils clean
	-$(MAKE) -C userspace/init clean
	-$(MAKE) -C userspace/shell clean
	-$(MAKE) -C userspace/login clean
	-$(MAKE) -C userspace/install clean
	-$(MAKE) -C desktop/wm clean
	-$(MAKE) -C desktop/panel clean
	-$(MAKE) -C desktop/fileman clean
	-$(MAKE) -C desktop/control clean
	-$(MAKE) -C desktop/sessionmgr clean
	-$(MAKE) -C tests/kernel clean
	-$(MAKE) -C tools/s-lang clean
	rm -f $(ISO) $(IMAGE)
	rm -rf build/isoroot build/lib build/include

# ── Help ─────────────────────────────────────────────────────────────────────
help:
	@echo "PhoenixOS build system"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build everything (default)"
	@echo "  kernel     - Build EmberKernel only"
	@echo "  libs       - Build runtime libraries (libflame, libgui, libipc)"
	@echo "  userspace  - Build userspace programs"
	@echo "  desktop    - Build AshDE desktop (requires libs)"
	@echo "  services   - Build Rust services (requires cargo)"
	@echo "  iso        - Create bootable ISO"
	@echo "  image      - Create raw disk image"
	@echo "  run        - Build and run in QEMU"
	@echo "  run-kvm    - Build and run in QEMU with KVM"
	@echo "  debug      - Start QEMU with GDB server"
	@echo "  test       - Run kernel unit tests"
	@echo "  sc         - Build S language compiler"
	@echo "  clean      - Remove all build artifacts"

