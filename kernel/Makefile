ARCH      := x86_64
CROSS_PREFIX := x86_64-elf-
CC        := $(CROSS_PREFIX)gcc
AS        := nasm
LD        := $(CROSS_PREFIX)ld
OBJCOPY   := $(CROSS_PREFIX)objcopy

CFLAGS    := -ffreestanding -O2 -Wall -Wextra -nostdlib -nostdinc \
             -fno-stack-protector -mno-red-zone -mcmodel=kernel \
             -std=c11 -I. -I..

ASFLAGS   := -f elf64
LDFLAGS   := -T ../build/kernel.ld -nostdlib

OUTPUT    := ember.elf

C_SRCS := \
    main.c \
    printk.c \
    lib/string.c \
    mm/pmm.c \
    mm/vmm.c \
    arch/x86_64/gdt.c \
    arch/x86_64/idt.c \
    arch/x86_64/interrupts.c \
    drivers/acpi/acpi.c \
    drivers/pci/pci.c \
    drivers/timer/pit.c \
    drivers/tty/serial.c \
    drivers/tty/console.c \
    drivers/fb/framebuffer.c \
    drivers/fb/gpu.c \
    drivers/input/keyboard.c \
    drivers/input/mouse.c \
    drivers/net/wifi.c \
    drivers/net/bluetooth.c \
    sched/scheduler.c \
    syscall/syscall.c \
    ipc/pipe.c \
    fs/vfs.c \
    fs/ext2.c \
    fs/procfs.c

ASM_SRCS := \
    arch/x86_64/boot/boot.asm \
    arch/x86_64/boot/startup.asm

C_OBJS   := $(C_SRCS:.c=.o)
ASM_OBJS := $(ASM_SRCS:.asm=.o)
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all clean

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo "[LD ] $@"
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	@echo "[CC ] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.asm
	@echo "[AS ] $<"
	$(AS) $(ASFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(OUTPUT)

-include $(C_OBJS:.o=.d)
