# Contributing to PhoenixOS

Thank you for your interest in contributing! PhoenixOS is a hobbyist OS project
welcoming contributors of all skill levels.

---

## Code Style

### C

- **Indent**: 4 spaces (no tabs)
- **Brace style**: K&R — opening brace on the same line as the statement

  ```c
  if (condition) {
      do_something();
  } else {
      do_other();
  }
  ```

- **Line length**: 100 characters max
- **Naming**:
  - Functions and variables: `snake_case`
  - Types / structs: `snake_case` with `_t` suffix for typedefs, or bare for
    `struct` definitions used via `struct foo`
  - Constants / macros: `UPPER_SNAKE_CASE`
  - File-local (static) functions: no special prefix, but use `static`

- **Comments**: use `/* ... */` for block comments; `//` for end-of-line notes.
  Document non-obvious logic, not obvious operations.

  ```c
  /* Walk the free list, coalescing adjacent free blocks. */
  for (blk = head; blk; blk = blk->next) {
      if (blk->free && blk->next && blk->next->free)
          coalesce(blk);
  }
  ```

- **Headers**: always include a header guard:

  ```c
  #ifndef KERNEL_MM_H
  #define KERNEL_MM_H
  /* ... */
  #endif /* KERNEL_MM_H */
  ```

- **Kernel code**: use `-ffreestanding` — no standard C library headers.
  Use types from `kernel/include/kernel.h` (`u8`, `u16`, `u32`, `u64`, etc.)

### Rust

- Follow standard Rust formatting: run `cargo fmt` before committing
- Run `cargo clippy` and address all warnings
- Prefer `?` over `.unwrap()` in library code
- Add doc-comments (`///`) for public functions

### Assembly (NASM)

- 4-space indent for instructions
- Labels at column 0, instructions indented
- Add a comment explaining any non-obvious instruction or macro

---

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short description>

[optional body]
```

Types:
- `feat`: new feature
- `fix`: bug fix
- `refactor`: code restructuring without behavior change
- `docs`: documentation only
- `style`: formatting, whitespace
- `test`: adding or fixing tests
- `build`: build system, Makefile, scripts
- `chore`: maintenance tasks

Scope (optional) should name the subsystem: `kernel`, `pmm`, `vmm`, `sched`,
`vfs`, `wm`, `libgui`, `pkgmgr`, etc.

Examples:

```
feat(sched): add priority aging to prevent starvation
fix(pmm): correct double-free detection in bitmap
docs(bootflow): add UEFI boot stage description
test(vmm): add mapping alignment assertions
```

---

## Areas Needing Help

### Kernel

- [ ] SMP (multi-core) scheduler
- [ ] ACPI power management (shutdown / reboot)
- [ ] tmpfs and procfs implementations
- [ ] Proper signal delivery (POSIX signals)
- [ ] Network stack (lwIP integration or custom)
- [ ] USB host controller (XHCI)

### Userspace & Shell

- [ ] PyreShell job control (`bg`, `fg`, `jobs`)
- [ ] More coreutils: `grep`, `sed`, `find`, `sort`, `uniq`
- [ ] `/etc/group` and multi-user permissions
- [ ] `cron`-like task scheduler

### Desktop / AshDE

- [ ] Window minimize / maximize
- [ ] Resize by dragging window border
- [ ] Copy/paste clipboard
- [ ] Program Manager
- [ ] Terminal emulator window (wraps PyreShell)
- [ ] Desktop icons and drag-and-drop
- [ ] System tray / notification area

### Libraries

- [ ] `libflame` math functions (`sin`, `cos`, `sqrt`, …)
- [ ] `libflame` networking (`socket`, `connect`, `recv`, `send`)
- [ ] `libgui` PNG/BMP image loading
- [ ] `libgui` TrueType font rasterizer

### Documentation

- [ ] Syscall reference manual
- [ ] `.phx` package format specification
- [ ] Hardware compatibility list
- [ ] Porting guide (how to cross-compile existing software)

---

## Submitting Patches

1. Fork the repository on GitHub
2. Create a feature branch: `git checkout -b feat/my-feature`
3. Make your changes with tests if applicable
4. Run existing tests: `make test` (or `cd tests && make`)
5. Commit with a conventional commit message
6. Push and open a Pull Request against `main`
7. Ensure CI checks pass
8. A maintainer will review within a week

---

## Development Environment

See [docs/BUILDING.md](BUILDING.md) for full setup instructions.

The fastest way to test a kernel change:

```bash
make kernel && scripts/run-qemu.sh
```

For iterative kernel debugging:

```bash
# Terminal 1:
scripts/run-qemu.sh --debug --wait-gdb

# Terminal 2:
scripts/debug.sh
```

---

## License

PhoenixOS is MIT licensed. By contributing, you agree your contributions will
be released under the same license. See [LICENSE](../LICENSE).
