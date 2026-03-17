# PhoenixOS Architecture

## Overview

PhoenixOS is a ground-up operating system built from scratch. It is **not** a
Linux distribution — it shares no userspace code with Linux and uses a custom
kernel (EmberKernel), custom init system (Kindle), custom shell (PyreShell),
and a custom desktop environment (AshDE).

```
┌────────────────────────────────────────────────────────────┐
│                   Applications / Desktop                    │
│   Scroll (files)   Forge (settings)   PyreShell (terminal) │
├────────────────────────────────────────────────────────────┤
│                     AshDE Desktop                           │
│  WM (window mgr)  Panel (taskbar)  Session Manager         │
├────────────────────────────────────────────────────────────┤
│               Runtime Libraries                             │
│  libflame (libc-like)  libgui  libipc  libruntime          │
├────────────────────────────────────────────────────────────┤
│              Core Userspace Services                        │
│  Kindle (init/PID 1)  phx-svcmon  phx-pkgmgr  phx-netd    │
├────────────────────────────────────────────────────────────┤
│                   System Call Interface                      │
│   read write open close fork exec mmap ioctl yield ...     │
├────────────────────────────────────────────────────────────┤
│                     EmberKernel                             │
│  Scheduler │ VFS │ PMM/VMM │ IPC │ Drivers │ Interrupts    │
├────────────────────────────────────────────────────────────┤
│               Hardware / Firmware                           │
│  BIOS/UEFI → GRUB2 → Multiboot2 → boot.asm                │
└────────────────────────────────────────────────────────────┘
```

---

## EmberKernel

EmberKernel is a **monolithic kernel** written in C with x86_64 assembly for
the architecture-specific boot and interrupt handling. It is inspired by Linux's
general structure but implemented independently without sharing any source code.

Key design decisions:
- **Monolithic**: all subsystems (drivers, VFS, scheduler) run in kernel space
- **Higher-half kernel**: loaded at `0xFFFFFFFF80100000` (−2 GB virtual)
- **Preemptive**: timer-driven context switching via the PIT
- **POSIX-like syscalls**: compatible enough for porting basic userspace software

---

## Memory Model

### Physical Memory Manager (PMM)

`kernel/mm/pmm.c` — bitmap allocator over the physical address space.

- Populated from the Multiboot2 memory map at boot time
- Each bit in the bitmap represents one 4 KB physical page
- `alloc_page()` scans for the first free bit and returns the physical address
- `free_page(phys_addr_t)` clears the corresponding bit

### Virtual Memory Manager (VMM)

`kernel/mm/vmm.c` — 4-level paging (PML4 → PDPT → PD → PT).

- Kernel maps itself at `KERNEL_BASE = 0xFFFFFFFF80000000`
- Identity mapping of the first 2 MB set up in `boot.asm` for early boot
- `map_page(phys, virt, flags)` walks/creates the page table hierarchy
- `unmap_page(virt)` removes a mapping and flushes the TLB (`invlpg`)

### Kernel Heap

`kmalloc` / `kfree` in `kernel/mm/vmm.c` implement a simple free-list allocator
over a 4 MB virtual region above the kernel BSS. Blocks carry a header
containing size and free/used state; adjacent free blocks are coalesced on free.

### User Heap

User processes get memory via `SYS_MMAP` (anonymous mapping) and manage their
own heap through the `malloc`/`free` implementation in `lib/libflame/memory.c`,
which calls `sbrk` internally.

---

## Process Model

### Task Structure

Defined in `kernel/include/sched.h`:

```c
struct task_struct {
    pid_t    pid;
    int      state;         /* TASK_RUNNING, TASK_SLEEPING, TASK_ZOMBIE ... */
    u8       priority;      /* 0 (idle) to 31 (realtime) */
    u64     *stack;         /* Kernel stack base */
    u64      rsp;           /* Saved stack pointer */
    u64      cr3;           /* Page table root (physical) */
    char     name[64];
    struct task_struct *next, *prev;
    /* ... open file table, signal masks, etc. */
};
```

### Scheduler

`kernel/sched/scheduler.c` implements a simple **round-robin** scheduler with
priority levels. The PIT fires at 100 Hz; each tick calls `sched_tick()`, which
decrements the current task's time slice. When the slice expires, `schedule()`
is called to pick the next `TASK_RUNNING` task with the highest priority.

Context switching uses `switch_context` (defined in `startup.asm`) to save and
restore all general-purpose registers plus the stack pointer.

---

## Filesystem Layer

### VFS (Virtual File System)

`kernel/fs/vfs.c` provides an abstraction over concrete filesystems:

- **Mount table**: up to 16 mount points, each associating a path with a
  `struct filesystem` driver instance
- **File descriptor table**: per-process table of up to 256 open `struct file`s
- **Path resolution**: `lookup_path()` splits the path by `/`, starts at the
  root dentry, and calls each filesystem's `lookup` operation for each component
- **Generic operations**: `vfs_open`, `vfs_read`, `vfs_write`, `vfs_close`
  delegate to the filesystem's `struct file_operations`

### ext2 Driver

`kernel/fs/ext2.c` provides read-only access to ext2 filesystems:

- Reads superblock and block group descriptor table
- Resolves inodes from inode numbers
- Reads directory entries
- Reads file data through the block pointer indirection chain

### Future Filesystems

- **PhoenixFS**: a custom journaling FS planned for Phase 5
- **tmpfs**: in-memory filesystem for `/tmp` and `/run`
- **procfs**: virtual FS exposing process information at `/proc`

---

## IPC

### Kernel Pipes

`kernel/ipc/pipe.c` provides anonymous pipes:

- 4 KB circular buffer per pipe
- Blocking read/write (tasks sleep when the pipe is empty/full)
- File-descriptor–based API mirroring POSIX `pipe(2)`

### Named IPC Channels (userspace)

`lib/libipc/` provides a Unix-domain-socket–based message-passing layer for
desktop components:

- `ipc_listen(name)` creates a named socket in `/run/ipc/`
- `ipc_connect(name, ch)` connects by name
- `ipc_send` / `ipc_recv` transfer fixed-size `Message` structs (max 256 bytes payload)
- The WM, panel, and services communicate through this layer

---

## Graphics Architecture

```
/dev/input/mouse ─────┐
/dev/input/keyboard ──┤──→ AshDE WM (desktop/wm/)
                       │       │
              libgui ──┘       ├─ compositor.c: back-buffer blit, dirty rects
                               │
                               ↓
                    /dev/fb0 (linear framebuffer)
                               │
                    QEMU VGA / real GPU framebuffer
```

1. **Framebuffer driver** (`kernel/drivers/fb/framebuffer.c`): exposes a memory-
   mapped region through `/dev/fb0` using the Multiboot2 framebuffer tag
2. **libgui** (`lib/libgui/`): per-window off-screen back-buffers, widget
   drawing with 8×16 bitmap glyphs, retro beveled 3D style
3. **Compositor** (`desktop/wm/compositor.c`): composites windows in z-order
   onto a full-screen back-buffer, then blits dirty rectangles to `/dev/fb0`

---

## Input Architecture

```
PS/2 keyboard ──→ IRQ1 ──→ kernel/drivers/input/keyboard.c
                                    ↓ circular buffer
                            /dev/input/keyboard (character device)
                                    ↓
                             AshDE WM reads & dispatches to focused window
```

---

## Security Model

- **Ring 0 / Ring 3 separation**: kernel code runs in ring 0; user processes in ring 3
- **Syscall gate**: `SYSCALL` / `SYSRET` instructions swap stacks and privilege levels
- **Login**: `userspace/login/login.c` reads `/etc/passwd`, checks a salted SHA-256
  hash, and calls `setuid`/`setgid` before launching the user's shell or desktop
- **Capabilities**: future work — basic UID/GID checks for now
- **Memory isolation**: each process has its own page table; kernel mappings are
  present in all address spaces but marked non-user-accessible (U/S=0)

---

## Component Interaction Diagram

```
Boot ROM → GRUB → boot.asm → kernel_start()
                                  │
             ┌────────────────────┼───────────────────────┐
             ▼                    ▼                        ▼
          GDT/IDT              PMM/VMM                 Serial console
             │                    │                        │
             └────────────────────┼───────────────────────┘
                                  ▼
                           PIT timer (100 Hz)
                                  │
                         Scheduler tick ──→ schedule()
                                  │
                           task: Kindle (PID 1)
                                  │
              ┌───────────────────┼─────────────────────┐
              ▼                   ▼                      ▼
       phx-svcmon            phx-pkgmgr            session mgr
              │                                         │
              └─────────────────────────────────────────┤
                                                         ▼
                                                  AshDE WM + Panel
                                                         │
                                                 /dev/fb0 (screen)
```
