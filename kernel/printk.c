#include "include/kernel.h"
#include "arch/x86_64/include/asm.h"

/* ── Serial port constants ────────────────────────────────────────────────── */

#define COM1_BASE   0x3F8
#define COM1_DATA   (COM1_BASE + 0)
#define COM1_IER    (COM1_BASE + 1)
#define COM1_FCR    (COM1_BASE + 2)
#define COM1_LCR    (COM1_BASE + 3)
#define COM1_MCR    (COM1_BASE + 4)
#define COM1_LSR    (COM1_BASE + 5)
#define LSR_THRE    0x20    /* Transmit Holding Register Empty */
#define LSR_DR      0x01    /* Data Ready */

/* ── Serial ready flag (set by serial_init in drivers/tty/serial.c) ─────── */

bool serial_ok = false;

/* Forward declaration for console output */
extern void console_putc(char c);
extern bool console_ready;

/* ── Serial transmit ──────────────────────────────────────────────────────── */

static void serial_putc_raw(char c) {
    if (!serial_ok) return;
    while (!(inb(COM1_LSR) & LSR_THRE))
        ;
    outb(COM1_DATA, (u8)c);
}

/* ── vsnprintk — internal formatter ──────────────────────────────────────── */

static void emit_char(char *buf, usize *pos, usize max, char c) {
    if (buf) {
        if (*pos < max - 1) buf[(*pos)] = c;
    } else {
        serial_putc_raw(c);
        if (console_ready) console_putc(c);
    }
    (*pos)++;
}

static void emit_str(char *buf, usize *pos, usize max, const char *s) {
    if (!s) s = "(null)";
    while (*s) emit_char(buf, pos, max, *s++);
}

static void emit_uint(char *buf, usize *pos, usize max,
                       u64 val, u8 base, bool upper, int width, char pad) {
    const char *digits_lower = "0123456789abcdef";
    const char *digits_upper = "0123456789ABCDEF";
    const char *digits = upper ? digits_upper : digits_lower;
    char tmp[64];
    int  len = 0;

    if (val == 0) {
        tmp[len++] = '0';
    } else {
        while (val) {
            tmp[len++] = digits[val % base];
            val /= base;
        }
    }
    /* pad to width */
    while (len < width) tmp[len++] = pad;
    /* reverse */
    for (int i = len - 1; i >= 0; i--)
        emit_char(buf, pos, max, tmp[i]);
}

static int vsnprintk(char *buf, usize max, const char *fmt, __builtin_va_list ap) {
    usize pos = 0;

    while (*fmt) {
        if (*fmt != '%') {
            emit_char(buf, &pos, max, *fmt++);
            continue;
        }
        fmt++; /* skip '%' */

        /* padding */
        char pad   = ' ';
        int  width = 0;

        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }

        bool is_long = false;
        if (*fmt == 'l') { is_long = true; fmt++; }
        if (*fmt == 'l') { fmt++; }  /* ll */

        switch (*fmt) {
        case 'd': {
            s64 v = is_long ? __builtin_va_arg(ap, s64)
                            : (s64)__builtin_va_arg(ap, s32);
            if (v < 0) { emit_char(buf, &pos, max, '-'); v = -v; }
            emit_uint(buf, &pos, max, (u64)v, 10, false, width, pad);
            break;
        }
        case 'u': {
            u64 v = is_long ? __builtin_va_arg(ap, u64)
                            : (u64)__builtin_va_arg(ap, u32);
            emit_uint(buf, &pos, max, v, 10, false, width, pad);
            break;
        }
        case 'x': {
            u64 v = is_long ? __builtin_va_arg(ap, u64)
                            : (u64)__builtin_va_arg(ap, u32);
            emit_uint(buf, &pos, max, v, 16, false, width, pad);
            break;
        }
        case 'X': {
            u64 v = is_long ? __builtin_va_arg(ap, u64)
                            : (u64)__builtin_va_arg(ap, u32);
            emit_uint(buf, &pos, max, v, 16, true, width, pad);
            break;
        }
        case 'p': {
            u64 v = (u64)__builtin_va_arg(ap, void *);
            emit_str(buf, &pos, max, "0x");
            emit_uint(buf, &pos, max, v, 16, false, 16, '0');
            break;
        }
        case 's': {
            const char *s = __builtin_va_arg(ap, const char *);
            emit_str(buf, &pos, max, s);
            break;
        }
        case 'c': {
            char c = (char)__builtin_va_arg(ap, int);
            emit_char(buf, &pos, max, c);
            break;
        }
        case '%':
            emit_char(buf, &pos, max, '%');
            break;
        default:
            emit_char(buf, &pos, max, '%');
            emit_char(buf, &pos, max, *fmt);
            break;
        }
        fmt++;
    }
    if (buf && max > 0) buf[pos < max ? pos : max - 1] = '\0';
    return (int)pos;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void early_printk(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintk(NULL, (usize)-1, fmt, ap);
    __builtin_va_end(ap);
}

void printk(const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintk(NULL, (usize)-1, fmt, ap);
    __builtin_va_end(ap);
}

int snprintk(char *buf, usize n, const char *fmt, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintk(buf, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}

void panic(const char *fmt, ...) {
    __asm__ volatile("cli");
    serial_putc_raw('\n');
    emit_str(NULL, &(usize){0}, (usize)-1, "\n*** KERNEL PANIC ***\n");

    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    vsnprintk(NULL, (usize)-1, fmt, ap);
    __builtin_va_end(ap);

    emit_str(NULL, &(usize){0}, (usize)-1, "\nSystem halted.\n");
    for (;;) __asm__ volatile("hlt");
}
