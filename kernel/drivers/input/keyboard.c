#include "../../include/kernel.h"
#include "../../arch/x86_64/include/asm.h"
#include "../../arch/x86_64/include/idt.h"

/* ── Keyboard buffer ──────────────────────────────────────────────────────── */

#define KB_BUF_SIZE  64

static volatile char kb_buf[KB_BUF_SIZE];
static volatile u32  kb_head = 0;
static volatile u32  kb_tail = 0;
static bool          shift_held  = false;
static bool          caps_lock   = false;

/* ── US QWERTY scancode set 1 → ASCII (unshifted) ────────────────────────── */

static const char scancode_table[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,    '*', 0,   ' ',
    /* caps lock = 0x3A, F keys etc */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0
};

static const char scancode_shift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    0,    '*', 0,   ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0
};

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_CAPSLOCK 0x3A
#define SC_RELEASE  0x80   /* bit 7 set = key released */

/* ── Enqueue character ────────────────────────────────────────────────────── */

static void kb_enqueue(char c) {
    u32 next = (kb_head + 1) % KB_BUF_SIZE;
    if (next != kb_tail) {
        kb_buf[kb_head] = c;
        kb_head = next;
    }
}

/* ── IRQ1 handler ─────────────────────────────────────────────────────────── */

static void keyboard_handler(struct interrupt_frame *frame) {
    (void)frame;
    u8 sc = inb(0x60);

    if (sc & SC_RELEASE) {
        u8 key = sc & ~SC_RELEASE;
        if (key == SC_LSHIFT || key == SC_RSHIFT) shift_held = false;
        return;
    }

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { shift_held = true;  return; }
    if (sc == SC_CAPSLOCK)                  { caps_lock = !caps_lock; return; }

    if (sc < 128) {
        char c;
        bool upper = shift_held ^ caps_lock;

        if (upper && scancode_shift[sc])
            c = scancode_shift[sc];
        else
            c = scancode_table[sc];

        if (c) kb_enqueue(c);
    }
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

void kb_init(void) {
    extern void irq_install_handler(u8 irq, void (*handler)(struct interrupt_frame *));
    irq_install_handler(1, keyboard_handler);
}

/* ── Public interface ─────────────────────────────────────────────────────── */

bool kb_pending(void) {
    return kb_head != kb_tail;
}

char kb_getc(void) {
    while (!kb_pending())
        __asm__ volatile("pause");
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}
