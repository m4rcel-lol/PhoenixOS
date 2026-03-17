# PhoenixOS Development Roadmap

## Phase 0 — Scaffolding ✅ (current)

**Goal**: Repository structure, build system, documentation, QEMU boot proof-of-concept.

- [x] Directory structure and initial files
- [x] Cross-compiler build script (`scripts/setup-cross.sh`)
- [x] Top-level Makefile with kernel / userspace / desktop targets
- [x] Kernel linker script (`build/kernel.ld`)
- [x] GRUB2 configuration (`boot/grub.cfg`)
- [x] Architecture documentation
- [x] Multiboot2 assembly entry point (`kernel/arch/x86_64/boot/boot.asm`)

---

## Phase 1 — Kernel Boot & Core Subsystems

**Goal**: A kernel that boots in QEMU, prints to serial, sets up memory and interrupts.

- [ ] Multiboot2 entry: long mode setup complete
- [ ] Serial console working (`kernel/drivers/tty/serial.c`)
- [ ] GDT + IDT initialization (`kernel/arch/x86_64/gdt.c`, `idt.c`)
- [ ] Exception handlers (0–31) with register dump
- [ ] 8259A PIC remapping and IRQ dispatch
- [ ] Physical memory manager from Multiboot2 map
- [ ] Virtual memory manager (4-level paging)
- [ ] Kernel heap (`kmalloc` / `kfree`)
- [ ] `printk` with basic format string support
- [ ] Framebuffer console (Multiboot2 framebuffer tag)
- [ ] PIT timer at 100 Hz

**Milestone**: `EmberKernel` prints boot banner on serial and screen in QEMU.

---

## Phase 2 — Scheduler, Syscalls & Init

**Goal**: Multi-tasking kernel with a running PID 1 (Kindle).

- [ ] Round-robin scheduler with priority levels
- [ ] Context switching (`switch_context` assembly stub)
- [ ] `task_create` / `task_exit` / `task_sleep`
- [ ] SYSCALL/SYSRET setup (STAR / LSTAR / SFMASK MSRs)
- [ ] Syscall dispatch table
- [ ] `sys_write`, `sys_exit`, `sys_getpid`, `sys_yield`
- [ ] Kindle init (PID 1): launches services from config
- [ ] Basic TTY (keyboard input → line buffer → process read)
- [ ] PyreShell running in a terminal task
- [ ] `/etc/passwd` login

**Milestone**: Boot to PyreShell prompt; type commands that run built-in helpers.

---

## Phase 3 — Filesystem & Storage

**Goal**: Read a real ext2 filesystem; userspace utilities working.

- [ ] VFS abstraction layer
- [ ] ext2 read-only driver
- [ ] Populate root filesystem image with userspace binaries
- [ ] `sys_open`, `sys_read`, `sys_close`, `sys_stat`, `sys_getdents`
- [ ] Coreutils: `ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `echo`, `pwd`
- [ ] PS/2 keyboard driver
- [ ] `/proc` virtual filesystem (basic: `/proc/version`, `/proc/<pid>/status`)
- [ ] Package format specification (`.phx`) finalized
- [ ] `phx-pkgmgr` install / list / remove

**Milestone**: Boot to shell; navigate filesystem, run basic commands, install a `.phx` package.

---

## Phase 4 — Framebuffer & AshDE Basics

**Goal**: Graphical desktop boots; windows can be created and moved.

- [ ] Framebuffer driver with double-buffering
- [ ] 8×16 PSF bitmap font renderer
- [ ] libgui: window creation, back-buffer, blit to framebuffer
- [ ] libgui: widget drawing (button, label, textbox, listbox)
- [ ] AshDE WM: window list, z-order, title bar decoration
- [ ] AshDE WM: mouse drag to move windows
- [ ] AshDE WM: close button
- [ ] Session manager: launches WM + panel after login
- [ ] Panel: clock, task buttons

**Milestone**: Graphical login → desktop with at least one window visible.

---

## Phase 5 — Full Desktop & Installer

**Goal**: Complete AshDE experience; disk installer.

- [ ] Scroll file manager (dual-pane)
- [ ] Forge control panel (display, appearance, system info)
- [ ] Themes: Retro Gray, Mono Classic, Dark Ember
- [ ] PyreShell terminal emulator window
- [ ] Program Manager (Windows 3.0-style icon groups)
- [ ] Desktop icons and right-click context menu
- [ ] Disk installer (guided partitioning, copy files, install GRUB)
- [ ] System sounds (PC speaker)
- [ ] Cursor themes

**Milestone**: Install PhoenixOS from ISO to a virtual disk; full desktop session.

---

## Phase 6 — Networking, SDK & Polish

**Goal**: Network connectivity, application SDK, developer tooling.

- [ ] Network driver (virtio-net for QEMU, RTL8139 for bare metal)
- [ ] TCP/IP stack (lwIP integration or custom minimal stack)
- [ ] `phx-netd` network daemon
- [ ] DNS resolver
- [ ] Simple web browser (text-mode or minimal GUI)
- [ ] PhoenixOS SDK: headers, libflame, cross-compiler wrapper
- [ ] Application packaging guide
- [ ] Ports: busybox, nano, vim (minimal), ncurses
- [ ] SMP support (multi-core scheduling)
- [ ] ACPI power management (shutdown, reboot via ACPI)

**Milestone**: Browse a website; install a ported application via `phx`.

---

## Future / Stretch Goals

- **PhoenixFS**: custom journaling filesystem
- **64-bit EFI boot** (UEFI + GOP framebuffer)
- **Audio**: AC97 / HDA driver, basic mixer
- **USB**: XHCI host controller, keyboard/mouse/storage
- **Virtualization**: basic KVM-like hypervisor
- **Security**: capability system, mandatory access control
- **Self-hosting**: build PhoenixOS on PhoenixOS
