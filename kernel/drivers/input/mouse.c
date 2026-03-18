#include "../../include/kernel.h"
#include "../../include/mouse.h"
#include "../../arch/x86_64/include/asm.h"
#include "../../arch/x86_64/include/idt.h"

/* ── PS/2 controller ports ───────────────────────────────────────────────── */

#define PS2_DATA    0x60   /* data port (R/W) */
#define PS2_STATUS  0x64   /* status register (R) */
#define PS2_CMD     0x64   /* command register (W) */

/* PS/2 status flags */
#define PS2_STATUS_OBF   0x01  /* output buffer full (data available to read) */
#define PS2_STATUS_IBF   0x02  /* input buffer full  (busy, don't write yet)  */
#define PS2_STATUS_AUX   0x20  /* data in output buffer is from aux port      */

/* PS/2 controller commands */
#define PS2_CMD_READ_CFG    0x20
#define PS2_CMD_WRITE_CFG   0x60
#define PS2_CMD_ENABLE_AUX  0xA8
#define PS2_CMD_WRITE_AUX   0xD4

/* Mouse commands */
#define MOUSE_CMD_RESET           0xFF
#define MOUSE_CMD_ENABLE_REPORT   0xF4
#define MOUSE_CMD_SET_DEFAULTS    0xF6
#define MOUSE_ACK                 0xFA

/* PS/2 config-byte bits */
#define PS2_CFG_AUX_IRQ  0x02   /* enable IRQ12 for auxiliary port */
#define PS2_CFG_AUX_CLK  0x20   /* auxiliary clock disable bit     */

/* ── Event ring buffer ───────────────────────────────────────────────────── */

#define MOUSE_BUF_SIZE  64

static volatile mouse_event_t mouse_buf[MOUSE_BUF_SIZE];
static volatile u32           mouse_head = 0;
static volatile u32           mouse_tail = 0;

/* ── PS/2 packet state machine ───────────────────────────────────────────── */

static volatile u8  packet[3];
static volatile u8  packet_idx = 0;

/* ── Low-level PS/2 helpers ──────────────────────────────────────────────── */

/* Wait until the controller input buffer is empty (safe to write) */
static void ps2_wait_write(void) {
    u32 timeout = 100000;
    while ((inb(PS2_STATUS) & PS2_STATUS_IBF) && --timeout)
        ;
}

/* Wait until the controller output buffer is full (data available) */
static bool ps2_wait_read(void) {
    u32 timeout = 100000;
    while (!(inb(PS2_STATUS) & PS2_STATUS_OBF) && --timeout)
        ;
    return timeout != 0;
}

/* Send a byte to the PS/2 controller command port */
static void ps2_send_cmd(u8 cmd) {
    ps2_wait_write();
    outb(PS2_CMD, cmd);
}

/* Send a byte to the PS/2 data port */
static void ps2_send_data(u8 data) {
    ps2_wait_write();
    outb(PS2_DATA, data);
}

/* Read a byte from the PS/2 data port (blocks briefly) */
static u8 ps2_read_data(void) {
    ps2_wait_read();
    return inb(PS2_DATA);
}

/* Route the next byte to the auxiliary (mouse) device */
static void mouse_send(u8 cmd) {
    ps2_send_cmd(PS2_CMD_WRITE_AUX);
    ps2_send_data(cmd);
}

/* ── IRQ12 handler ───────────────────────────────────────────────────────── */

static void mouse_irq_handler(struct interrupt_frame *frame) {
    (void)frame;

    u8 status = inb(PS2_STATUS);
    if (!(status & PS2_STATUS_OBF) || !(status & PS2_STATUS_AUX))
        return;

    u8 byte = inb(PS2_DATA);

    /* First byte must have bit 3 set; resync if it doesn't */
    if (packet_idx == 0 && !(byte & 0x08))
        return;

    packet[packet_idx++] = byte;

    if (packet_idx == 3) {
        packet_idx = 0;

        /* Decode 3-byte standard PS/2 mouse packet */
        u8  flags = packet[0];
        s8  dx    = (s8)packet[1];
        s8  dy    = (s8)packet[2];

        /* Overflow: discard packet */
        if ((flags & 0x40) || (flags & 0x80))
            return;

        mouse_event_t evt;
        evt.dx         = dx;
        evt.dy         = (s8)(-dy);   /* PS/2 y-axis is inverted vs. screen */
        evt.btn_left   = (flags & 0x01) != 0;
        evt.btn_right  = (flags & 0x02) != 0;
        evt.btn_middle = (flags & 0x04) != 0;

        u32 next = (mouse_head + 1) % MOUSE_BUF_SIZE;
        if (next != mouse_tail) {
            mouse_buf[mouse_head] = evt;
            mouse_head = next;
        }
    }
}

/* ── mouse_init ──────────────────────────────────────────────────────────── */

void mouse_init(void) {
    extern void irq_install_handler(u8 irq,
                                    void (*handler)(struct interrupt_frame *));

    /* Enable the auxiliary device */
    ps2_send_cmd(PS2_CMD_ENABLE_AUX);

    /* Read current controller configuration byte */
    ps2_send_cmd(PS2_CMD_READ_CFG);
    u8 cfg = ps2_read_data();

    /* Enable IRQ12 and re-enable auxiliary clock */
    cfg |=  PS2_CFG_AUX_IRQ;
    cfg &= ~PS2_CFG_AUX_CLK;

    ps2_send_cmd(PS2_CMD_WRITE_CFG);
    ps2_send_data(cfg);

    /* Reset and configure the mouse */
    mouse_send(MOUSE_CMD_RESET);
    ps2_read_data();   /* ACK */
    ps2_read_data();   /* 0xAA (BAT passed) */
    ps2_read_data();   /* device ID (0x00 = standard mouse) */

    mouse_send(MOUSE_CMD_SET_DEFAULTS);
    ps2_read_data();   /* ACK */

    /* Enable data reporting */
    mouse_send(MOUSE_CMD_ENABLE_REPORT);
    ps2_read_data();   /* ACK */

    /* Install IRQ12 handler */
    irq_install_handler(12, mouse_irq_handler);

    printk("[mouse] PS/2 mouse initialized\n");
}

/* ── Public interface ────────────────────────────────────────────────────── */

bool mouse_pending(void) {
    return mouse_head != mouse_tail;
}

bool mouse_read(mouse_event_t *evt) {
    if (!mouse_pending())
        return false;
    *evt = mouse_buf[mouse_tail];
    mouse_tail = (mouse_tail + 1) % MOUSE_BUF_SIZE;
    return true;
}
