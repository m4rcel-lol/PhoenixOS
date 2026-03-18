OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VBASE = 0xFFFFFFFF80000000;
KERNEL_LOAD  = 0x0000000000100000;

PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R-X */
    rodata PT_LOAD FLAGS(4);   /* R-- */
    data   PT_LOAD FLAGS(6);   /* RW- */
}

SECTIONS
{
    . = KERNEL_VBASE + KERNEL_LOAD;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_VBASE)
    {
        /* Multiboot2 header must be within first 32KB of image */
        KEEP(*(.multiboot2))
        *(.text)
        *(.text.*)
    } :text

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_VBASE)
    {
        *(.rodata)
        *(.rodata.*)
    } :rodata

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_VBASE)
    {
        *(.data)
        *(.data.*)
    } :data

    .bss ALIGN(4096) : AT(ADDR(.bss) - KERNEL_VBASE)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note.GNU-stack)
    }
}
