; boot.asm - EmberKernel Multiboot2 entry point
; Handles: multiboot2 header, 32-bit protected mode entry from GRUB,
;           long mode setup (4-level paging, EFER), jump to 64-bit kernel_start

bits 32

KERNEL_VBASE    equ 0xFFFFFFFF80000000
KERNEL_PBASE    equ 0x0000000000100000
PAGE_PRESENT    equ (1 << 0)
PAGE_WRITE      equ (1 << 1)
PAGE_HUGE       equ (1 << 7)

; ─── Multiboot2 Header ──────────────────────────────────────────────────────
section .multiboot2
align 8
mb2_header:
    dd  0xe85250d6                          ; magic
    dd  0                                   ; architecture: i386 protected mode
    dd  mb2_header_end - mb2_header         ; header length
    dd  -(0xe85250d6 + 0 + (mb2_header_end - mb2_header))  ; checksum

    ; Framebuffer tag (request linear framebuffer)
    align 8
    dw  5                                   ; type = framebuffer
    dw  1                                   ; flags (optional)
    dd  20                                  ; size
    dd  0                                   ; width  (0 = no preference)
    dd  0                                   ; height (0 = no preference)
    dd  32                                  ; depth  (32 bpp)

    ; Entry address tag — tell GRUB the PHYSICAL address to jump to.
    ; GRUB runs in 32-bit protected mode and cannot reach the higher-half
    ; virtual address stored in the ELF e_entry field; this tag overrides it.
    align 8
    dw  3                                   ; type = entry_address
    dw  0                                   ; flags (required = 0)
    dd  12                                  ; size
    dd  _start - KERNEL_VBASE               ; physical entry point

    ; End tag
    align 8
    dw  0                                   ; type = end
    dw  0                                   ; flags
    dd  8                                   ; size
mb2_header_end:

; ─── 32-bit Entry ───────────────────────────────────────────────────────────
section .text
global _start
_start:
    ; GRUB gives us: EAX = 0x36d76289 (multiboot2 magic), EBX = multiboot info ptr
    cli

    ; Save multiboot info pointer (EBX) – will pass to kernel as argument
    mov  edi, eax                           ; arg1: magic
    mov  esi, ebx                           ; arg2: multiboot info ptr

    ; Set up a temporary 32-bit stack
    mov  esp, stack32_top - KERNEL_VBASE

    ; Verify CPUID support (try toggling ID bit in EFLAGS)
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1 << 21)
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    cmp  eax, ecx
    je   .no_cpuid

    ; Check long mode support via CPUID extended feature
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_longmode
    mov  eax, 0x80000001
    cpuid
    test edx, (1 << 29)                    ; LM bit
    jz   .no_longmode

    ; ── Set up 4-level paging ────────────────────────────────────────────────
    ; Zero page tables area
    mov  edi, pml4_table - KERNEL_VBASE
    mov  ecx, 4096 * 3 / 4                 ; PML4 + PDPT + PD = 3 pages
    xor  eax, eax
    rep  stosd

    ; PML4[0]  -> PDPT_low  (identity map)
    ; PML4[511]-> PDPT_high (higher-half kernel)
    mov  eax, pdpt_low - KERNEL_VBASE
    or   eax, (PAGE_PRESENT | PAGE_WRITE)
    mov  [pml4_table - KERNEL_VBASE], eax
    mov  [pml4_table - KERNEL_VBASE + 511 * 8], eax       ; same PDPT for higher half

    ; PDPT[0]  -> PD[0] (covers 0 – 1 GB, identity mapping)
    ; PDPT[510]-> PD[0] (covers 0xFFFFFFFF80000000, higher-half kernel)
    mov  eax, pd_table - KERNEL_VBASE
    or   eax, (PAGE_PRESENT | PAGE_WRITE)
    mov  [pdpt_low - KERNEL_VBASE], eax
    mov  [pdpt_low - KERNEL_VBASE + 510 * 8], eax         ; higher-half PDPT entry

    ; PD[0] = 0x00000000..0x001FFFFF (2 MB huge page, covers kernel load addr)
    mov  dword [pd_table - KERNEL_VBASE], (0x00000000 | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE)
    ; PD[1] = 0x00200000..0x003FFFFF
    mov  dword [pd_table - KERNEL_VBASE + 8], (0x00200000 | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE)
    ; Map a few more for safety
    mov  dword [pd_table - KERNEL_VBASE + 16], (0x00400000 | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE)
    mov  dword [pd_table - KERNEL_VBASE + 24], (0x00600000 | PAGE_PRESENT | PAGE_WRITE | PAGE_HUGE)

    ; Load PML4 into CR3
    mov  eax, pml4_table - KERNEL_VBASE
    mov  cr3, eax

    ; Enable PAE in CR4
    mov  eax, cr4
    or   eax, (1 << 5)                     ; PAE bit
    mov  cr4, eax

    ; Enable long mode in EFER MSR
    mov  ecx, 0xC0000080                   ; EFER MSR
    rdmsr
    or   eax, (1 << 8)                     ; LME bit
    wrmsr

    ; Enable paging + protected mode in CR0 (paging bit activates long mode)
    mov  eax, cr0
    or   eax, (1 << 31) | (1 << 0)        ; PG | PE
    mov  cr0, eax

    ; Load 64-bit GDT and far jump to 64-bit code segment
    ; Use physical addresses here – we are still in 32-bit protected mode before
    ; paging is active, so virtual addresses > 4 GB cannot be encoded in 32-bit
    ; relocations.  gdt64_ptr_phys stores the GDT base as a 32-bit physical
    ; address (gdt64 - KERNEL_VBASE); similarly the far-jump target is the
    ; physical address of long_mode_entry.
    lgdt [gdt64_ptr_phys - KERNEL_VBASE]
    jmp  0x08:(long_mode_entry - KERNEL_VBASE) ; CS selector = 0x08

.no_cpuid:
    mov  al, 'C'
    jmp  .error_halt
.no_longmode:
    mov  al, 'L'
.error_halt:
    mov  byte [0xB8000], al
    mov  byte [0xB8001], 0x4F              ; red background, white text
.halt:
    hlt
    jmp  .halt

; ─── 64-bit Entry ───────────────────────────────────────────────────────────
bits 64
long_mode_entry:
    ; Reload data segments
    mov  ax, 0x10                          ; kernel data selector
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Switch to proper 64-bit stack
    mov  rsp, stack_top

    ; Arguments: RDI = multiboot magic, RSI = multiboot info ptr
    ; (already set in 32-bit entry as EDI/ESI, zero-extended by transition)

    ; Jump to kernel C entry (linked at high virtual address)
    mov  rax, kernel_start
    call rax

    ; Should never return
.hang:
    cli
    hlt
    jmp  .hang

; ─── GDT for Long Mode ──────────────────────────────────────────────────────
section .rodata
align 16
gdt64:
    dq  0x0000000000000000                 ; 0x00 null
    dq  0x00AF9A000000FFFF                 ; 0x08 kernel code 64-bit (L=1, P=1, DPL=0, S=1, E=1, R=1)
    dq  0x00CF92000000FFFF                 ; 0x10 kernel data (P=1, DPL=0, S=1, W=1)
gdt64_end:

gdt64_ptr:
    dw  gdt64_end - gdt64 - 1
    dq  gdt64

; Physical-address GDT pointer used by the 32-bit setup code before paging.
; dd (32-bit) is used for the base so the relocation (R_X86_64_32) fits in the
; 32-bit address space: gdt64_phys = gdt64_virt - KERNEL_VBASE < 4 GB.
gdt64_ptr_phys:
    dw  gdt64_end - gdt64 - 1
    dd  gdt64 - KERNEL_VBASE

; ─── BSS / Stacks / Page Tables ─────────────────────────────────────────────
section .bss
align 4096

global pml4_table
pml4_table:
    resb 4096

pdpt_low:
    resb 4096

pd_table:
    resb 4096

; 32-bit bootstrap stack
align 16
stack32:
    resb 4096
stack32_top:

; 64-bit boot stack (16 KB)
align 16
stack_bottom:
    resb 16384
stack_top:

; External symbol from kernel C
extern kernel_start
