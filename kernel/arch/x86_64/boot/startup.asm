; startup.asm - 64-bit kernel stubs
; Interrupt entry stubs, syscall entry, and context switch

bits 64
section .text

; ─── Interrupt Frame Layout (pushed onto stack) ──────────────────────────────
; [RSP+0]  = error code (or 0 for exceptions without one)
; [RSP+8]  = RIP   (pushed by CPU)
; [RSP+16] = CS
; [RSP+24] = RFLAGS
; [RSP+32] = RSP (user)
; [RSP+40] = SS  (user)

; save_regs / restore_regs macros for general-purpose registers
%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro RESTORE_REGS 0
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rdx
    pop  rcx
    pop  rbx
    pop  rax
%endmacro

; ─── Exception stubs (0-31) ─────────────────────────────────────────────────
; Some exceptions push an error code automatically; for those that don't we push 0.

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0                           ; dummy error code
    push qword %1                          ; interrupt number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
                                           ; error code already on stack by CPU
    push qword %1                          ; interrupt number
    jmp  isr_common_stub
%endmacro

ISR_NOERR 0    ; #DE Divide Error
ISR_NOERR 1    ; #DB Debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; #BP Breakpoint
ISR_NOERR 4    ; #OF Overflow
ISR_NOERR 5    ; #BR Bound Range Exceeded
ISR_NOERR 6    ; #UD Invalid Opcode
ISR_NOERR 7    ; #NM Device Not Available
ISR_ERR   8    ; #DF Double Fault
ISR_NOERR 9    ; Coprocessor Segment Overrun (legacy)
ISR_ERR   10   ; #TS Invalid TSS
ISR_ERR   11   ; #NP Segment Not Present
ISR_ERR   12   ; #SS Stack-Segment Fault
ISR_ERR   13   ; #GP General Protection Fault
ISR_ERR   14   ; #PF Page Fault
ISR_NOERR 15   ; reserved
ISR_NOERR 16   ; #MF x87 FPU Error
ISR_ERR   17   ; #AC Alignment Check
ISR_NOERR 18   ; #MC Machine Check
ISR_NOERR 19   ; #XF SIMD FP Exception
ISR_NOERR 20   ; #VE Virtualization Exception
ISR_NOERR 21   ; reserved
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30   ; #SX Security Exception
ISR_NOERR 31   ; reserved

; ─── IRQ stubs (32-47, mapped from PIC IRQ0-15) ─────────────────────────────
%macro IRQ 2
global irq%1
irq%1:
    push qword 0
    push qword %2
    jmp  irq_common_stub
%endmacro

IRQ 0,  32   ; PIT timer
IRQ 1,  33   ; PS/2 keyboard
IRQ 2,  34   ; cascade (internal)
IRQ 3,  35   ; COM2
IRQ 4,  36   ; COM1
IRQ 5,  37   ; LPT2
IRQ 6,  38   ; floppy
IRQ 7,  39   ; LPT1 / spurious
IRQ 8,  40   ; RTC
IRQ 9,  41   ; ACPI
IRQ 10, 42   ; free
IRQ 11, 43   ; free
IRQ 12, 44   ; PS/2 mouse
IRQ 13, 45   ; FPU
IRQ 14, 46   ; ATA primary
IRQ 15, 47   ; ATA secondary

; ─── Common exception stub ──────────────────────────────────────────────────
extern exception_dispatch
isr_common_stub:
    SAVE_REGS
    mov  rdi, rsp                          ; pointer to full register frame
    call exception_dispatch
    RESTORE_REGS
    add  rsp, 16                           ; pop int number + error code
    iretq

; ─── Common IRQ stub ────────────────────────────────────────────────────────
extern irq_dispatch
irq_common_stub:
    SAVE_REGS
    mov  rdi, rsp
    call irq_dispatch
    RESTORE_REGS
    add  rsp, 16
    iretq

; ─── SYSCALL entry stub ─────────────────────────────────────────────────────
; SYSCALL leaves: RCX = return RIP, R11 = saved RFLAGS
; Arguments per SysV ABI already in RDI, RSI, RDX, R10 (instead of RCX), R8, R9
; Syscall number in RAX.
extern syscall_handler
global syscall_entry
syscall_entry:
    ; swap to kernel stack via per-CPU GS or fixed address
    ; For simplicity in early boot, we use a fixed kernel stack pointer stored
    ; in the TSS RSP0 – the CPU doesn't do that for SYSCALL, so we do it here.
    swapgs
    mov  [gs:8], rsp                       ; save user RSP
    mov  rsp,    [gs:0]                    ; load kernel RSP

    push rcx                               ; save user RIP
    push r11                               ; save user RFLAGS
    push rbp

    ; Pass syscall number and args
    mov  rcx, r10                          ; 4th arg: r10 -> rcx (SysV)
    ; rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4, r8=arg5, r9=arg6, rax=syscall#
    call syscall_handler

    pop  rbp
    pop  r11
    pop  rcx

    mov  rsp, [gs:8]                       ; restore user RSP
    swapgs
    sysretq

; ─── Context switch ─────────────────────────────────────────────────────────
; switch_context(u64** old_rsp_ptr, u64* new_rsp)
;   RDI = pointer to old task's rsp field
;   RSI = new task's saved rsp value
global switch_context
switch_context:
    ; Save callee-saved registers on current stack
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save current RSP into *old_rsp_ptr
    mov  [rdi], rsp

    ; Switch to new stack
    mov  rsp, rsi

    ; Restore new task's callee-saved registers
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbx
    pop  rbp

    ret
