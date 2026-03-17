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

.PHONY: all kernel userspace desktop image iso run debug clean

all: kernel userspace desktop iso

kernel:
	$(MAKE) -C kernel

userspace:
	$(MAKE) -C userspace/coreutils
	$(MAKE) -C userspace/init
	$(MAKE) -C userspace/shell
	$(MAKE) -C userspace/login

desktop:
	$(MAKE) -C desktop/wm
	$(MAKE) -C desktop/panel
	$(MAKE) -C desktop/fileman
	$(MAKE) -C desktop/control

iso: kernel userspace desktop
	@echo "[ISO] Building $(ISO)..."
	@bash scripts/make-image.sh

image: iso
	@echo "[IMG] Building disk image $(IMAGE)..."
	@dd if=/dev/zero of=$(IMAGE) bs=1M count=512 status=none
	@parted -s $(IMAGE) mklabel msdos mkpart primary 2048s 100%
	@mkfs.ext2 -F -b 4096 $(IMAGE)

run: iso
	$(QEMU) $(QEMU_FLAGS) $(QEMU_CDROM)

run-kvm: iso
	$(QEMU) $(QEMU_FLAGS) $(QEMU_CDROM) -enable-kvm

debug: iso
	@bash scripts/debug.sh

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C userspace/coreutils clean
	$(MAKE) -C userspace/init clean
	$(MAKE) -C userspace/shell clean
	$(MAKE) -C userspace/login clean
	$(MAKE) -C desktop/wm clean
	$(MAKE) -C desktop/panel clean
	$(MAKE) -C desktop/fileman clean
	$(MAKE) -C desktop/control clean
	rm -f $(ISO) $(IMAGE)
	rm -rf build/isoroot

help:
	@echo "PhoenixOS build system"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build everything (default)"
	@echo "  kernel     - Build EmberKernel only"
	@echo "  userspace  - Build userspace programs"
	@echo "  desktop    - Build AshDE desktop"
	@echo "  iso        - Create bootable ISO"
	@echo "  run        - Build and run in QEMU"
	@echo "  run-kvm    - Build and run in QEMU with KVM"
	@echo "  debug      - Start QEMU with GDB server"
	@echo "  clean      - Remove build artifacts"
