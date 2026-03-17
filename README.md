# PhoenixOS

**PhoenixOS** is a custom operating system built from scratch, featuring a Linux-derived monolithic kernel called **EmberKernel**, a retro Windows 1.0/3.0-inspired desktop environment called **AshDE**, and a complete userspace stack — all written in C and Rust.

This is **not a Linux distribution**. PhoenixOS uses its own kernel, init system, libc, shell, package format, and desktop environment.

---

## Components

| Component | Name | Language | Description |
|-----------|------|----------|-------------|
| Kernel | EmberKernel | C + NASM | Custom monolithic kernel |
| Init system | Kindle | C | PID 1 service supervisor |
| Shell | PyreShell (`pyre`) | C | POSIX-inspired command shell |
| libc | libflame | C | Userspace C runtime library |
| Desktop | AshDE | C | Retro tiling/floating WM |
| File Manager | Scroll | C | Dual-pane file manager |
| Control Panel | Forge | C | System settings GUI |
| Package format | `.phx` | — | Phoenix Package (tar+zstd) |
| Package manager | `phx` | Rust | Package installer/manager |
| Service monitor | phx-svcmon | Rust | Background service supervisor |

---

## Quick Start

### Prerequisites

- `x86_64-elf` cross-compiler (GCC + binutils)
- NASM assembler
- QEMU (`qemu-system-x86_64`)
- GRUB 2 (`grub-mkrescue`)
- Rust toolchain with `x86_64-unknown-none` target

### Setup Cross-Compiler

```bash
bash scripts/setup-cross.sh
export PATH="$HOME/opt/cross/bin:$PATH"
```

### Build

```bash
make all
```

### Run in QEMU

```bash
bash scripts/run-qemu.sh
# or:
make run
```

### Debug with GDB

```bash
bash scripts/debug.sh
```

---

## Project Structure

```
PhoenixOS/
├── boot/           GRUB configuration
├── kernel/         EmberKernel source
│   ├── arch/x86_64 Architecture-specific code (boot, GDT, IDT, interrupts)
│   ├── mm/         Memory management (PMM, VMM, heap)
│   ├── sched/      Scheduler
│   ├── fs/         VFS + ext2 driver
│   ├── drivers/    TTY, framebuffer, keyboard, timer
│   ├── syscall/    System call interface
│   └── ipc/        Inter-process communication
├── userspace/      Userspace programs
│   ├── init/       Kindle init system
│   ├── shell/      PyreShell
│   ├── coreutils/  Core Unix-like utilities
│   ├── login/      Login manager
│   └── services/   Rust system services
├── lib/            Runtime libraries
│   ├── libflame/   libc-like runtime
│   ├── libgui/     GUI widget toolkit
│   └── libipc/     IPC client library
├── desktop/        AshDE desktop environment
│   ├── wm/         Window manager
│   ├── panel/      Taskbar/panel
│   ├── fileman/    Scroll file manager
│   ├── control/    Forge control panel
│   └── themes/     UI themes
├── scripts/        Build and utility scripts
├── docs/           Documentation
└── tests/          Unit and integration tests
```

---

## Documentation

- [Architecture Overview](docs/ARCHITECTURE.md)
- [Boot Flow](docs/BOOTFLOW.md)
- [Building](docs/BUILDING.md)
- [Roadmap](docs/ROADMAP.md)
- [GUI Design](docs/GUI_DESIGN.md)
- [Contributing](docs/CONTRIBUTING.md)

---

## License

MIT — see [LICENSE](LICENSE).
