#include "../../include/kernel.h"
#include "../../arch/x86_64/include/asm.h"

/* ── COM1 port addresses ──────────────────────────────────────────────────── */

#define COM1_BASE  0x3F8
#define COM1_DATA  (COM1_BASE + 0)
#define COM1_IER   (COM1_BASE + 1)   /* interrupt enable */
#define COM1_FCR   (COM1_BASE + 2)   /* FIFO control */
#define COM1_LCR   (COM1_BASE + 3)   /* line control */
#define COM1_MCR   (COM1_BASE + 4)   /* modem control */
#define COM1_LSR   (COM1_BASE + 5)   /* line status */
#define COM1_MSR   (COM1_BASE + 6)   /* modem status */

/* LCR bits */
#define LCR_DLAB  0x80   /* divisor latch access bit */
#define LCR_8N1   0x03   /* 8 data bits, no parity, 1 stop bit */

/* LSR bits */
#define LSR_THRE  0x20   /* transmit holding register empty */
#define LSR_DR    0x01   /* data ready */

/* Baud rate divisor for 115200 from 1.8432 MHz base */
#define BAUD_115200  1

/* ── Init ─────────────────────────────────────────────────────────────────── */

void serial_init(void) {
    outb(COM1_IER, 0x00);                    /* disable all interrupts */
    outb(COM1_LCR, LCR_DLAB);               /* enable divisor latch */
    outb(COM1_DATA, BAUD_115200 & 0xFF);     /* divisor low byte */
    outb(COM1_IER,  (BAUD_115200 >> 8) & 0xFF); /* divisor high byte */
    outb(COM1_LCR, LCR_8N1);                /* 8N1, clear DLAB */
    outb(COM1_FCR, 0xC7);                   /* enable+clear FIFO, 14-byte threshold */
    outb(COM1_MCR, 0x0B);                   /* RTS+DTR+OUT2 */
}

/* ── Transmit ─────────────────────────────────────────────────────────────── */

void serial_putc(char c) {
    /* Wait until transmit holding register is empty */
    while (!(inb(COM1_LSR) & LSR_THRE))
        ;
    if (c == '\n') {
        outb(COM1_DATA, '\r');
        while (!(inb(COM1_LSR) & LSR_THRE))
            ;
    }
    outb(COM1_DATA, (u8)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

/* ── Receive ──────────────────────────────────────────────────────────────── */

bool serial_received(void) {
    return !!(inb(COM1_LSR) & LSR_DR);
}

char serial_getc(void) {
    while (!serial_received())
        ;
    return (char)inb(COM1_DATA);
}
