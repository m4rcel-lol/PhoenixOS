# Building PhoenixOS

## Prerequisites

| Tool | Version | Purpose |
|------|---------|---------|
| `x86_64-elf-gcc` | ≥ 13.0 | Cross-compiler for kernel and userspace |
| `x86_64-elf-ld`  | ≥ 2.40 | Linker (part of binutils) |
| `nasm`           | ≥ 2.15 | Assembler for boot stubs and interrupts |
| `grub-mkrescue`  | ≥ 2.06 | Creates bootable ISO image |
| `grub-pc-bin`    | ≥ 2.06 | BIOS GRUB modules for ISO |
| `qemu-system-x86_64` | ≥ 7.0 | Run in a virtual machine |
| `cargo` / `rustup` | stable | Build Rust userspace services |
| `xorriso`        | ≥ 1.5 | ISO creation (dependency of grub-mkrescue) |
| `make`           | ≥ 4.0 | Build system |

### Install dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential nasm make \
    grub-pc-bin grub-common xorriso \
    qemu-system-x86 \
    gdb-multiarch     # for debugging
```

### Install Rust

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

---

## Cross-Compiler Setup

PhoenixOS requires an `x86_64-elf` (freestanding) cross-compiler.
The provided script automates this:

```bash
chmod +x scripts/setup-cross.sh
./scripts/setup-cross.sh
```

This will:
1. Download and build **binutils** `x86_64-elf` (ld, objdump, objcopy, …)
2. Download and build **GCC** `x86_64-elf` (no hosted libraries)
3. Install everything to `$HOME/opt/cross`
4. Add the Rust target `x86_64-unknown-none`

Build time: ~15–30 minutes depending on hardware.

After the script finishes, add the cross-compiler to your `PATH`:

```bash
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
export PATH="$HOME/opt/cross/bin:$PATH"
```

Verify:

```bash
x86_64-elf-gcc --version
# x86_64-elf-gcc (GCC) 13.2.0
```

---

## Building

### Build everything

```bash
make all
```

This runs `make kernel`, `make userspace`, then builds Rust services.

### Build only the kernel

```bash
make kernel
# Output: kernel/ember.elf
```

### Build only userspace

```bash
make userspace
# Output: build/userspace/
```

### Build Rust services manually

```bash
cd userspace/services
cargo build --release
```

### Using the build script

The build script provides coloured progress output and pre-flight checks:

```bash
scripts/build.sh
```

---

## Creating a Bootable Image

```bash
scripts/make-image.sh
```

This creates:
- `phoenix.iso` — bootable CD-ROM ISO (for QEMU `-cdrom`)
- `phoenix.img` — raw disk image with ext2 partition (requires root for loop device)

---

## Running in QEMU

```bash
scripts/run-qemu.sh
```

Options:

```bash
scripts/run-qemu.sh --memory=512M   # more RAM
scripts/run-qemu.sh --kvm           # hardware acceleration (Linux host, Intel/AMD)
scripts/run-qemu.sh --debug         # enable GDB stub on :1234
```

Or run QEMU directly:

```bash
qemu-system-x86_64 \
    -m 256M \
    -cdrom phoenix.iso \
    -boot d \
    -serial stdio \
    -vga std \
    -no-reboot -no-shutdown
```

---

## Debugging with GDB

Start QEMU with the GDB stub, then connect GDB in another terminal:

**Terminal 1:**

```bash
scripts/run-qemu.sh --debug --wait-gdb
# QEMU waits; nothing boots until GDB connects
```

**Terminal 2:**

```bash
scripts/debug.sh
# Starts GDB, connects to :1234, loads kernel symbols
```

Or connect manually:

```bash
gdb-multiarch kernel/ember.elf
(gdb) set architecture i386:x86-64
(gdb) target remote :1234
(gdb) break kernel_start
(gdb) continue
```

---

## Cleaning

```bash
make clean
# or
scripts/clean.sh
```

---

## Build Output Files

| Path | Description |
|------|-------------|
| `kernel/ember.elf` | EmberKernel ELF binary |
| `build/isoroot/` | ISO staging directory |
| `phoenix.iso` | Bootable ISO image |
| `phoenix.img` | Raw disk image |
| `userspace/services/target/release/phx-pkgmgr` | Package manager binary |
| `userspace/services/target/release/phx-svcmon` | Service monitor binary |
