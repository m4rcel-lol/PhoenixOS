#include "../../include/kernel.h"

/* ── Console state ────────────────────────────────────────────────────────── */

#define CONSOLE_COLS  80
#define CONSOLE_ROWS  25
#define TAB_WIDTH      8

static u32  con_col  = 0;
static u32  con_row  = 0;
static u8   con_fg   = 7;   /* light gray */
static u8   con_bg   = 0;   /* black */

bool console_ready = false;

/* ── Framebuffer interface ────────────────────────────────────────────────── */

extern void fb_draw_char_bitmap(u32 x, u32 y, char c, u32 fg_color, u32 bg_color);
extern void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
extern void fb_scroll_up(u32 lines);
extern bool fb_ready;

/* Standard VGA-style 16-color palette (24-bit RGB) */
static const u32 vga_colors[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

#define CHAR_W  8
#define CHAR_H  16

/* ── Scroll ───────────────────────────────────────────────────────────────── */

static void console_scroll(void) {
    if (fb_ready) {
        fb_scroll_up(CHAR_H);
    }
    con_row = CONSOLE_ROWS - 1;
}

/* ── console_putc ─────────────────────────────────────────────────────────── */

void console_putc(char c) {
    if (!console_ready || !fb_ready) return;

    if (c == '\n') {
        con_col = 0;
        con_row++;
    } else if (c == '\r') {
        con_col = 0;
    } else if (c == '\t') {
        u32 next = (con_col + TAB_WIDTH) & ~(TAB_WIDTH - 1);
        while (con_col < next && con_col < CONSOLE_COLS) {
            fb_draw_char_bitmap(con_col * CHAR_W, con_row * CHAR_H,
                                ' ', vga_colors[con_fg], vga_colors[con_bg]);
            con_col++;
        }
    } else if (c == '\b') {
        if (con_col > 0) {
            con_col--;
            fb_draw_char_bitmap(con_col * CHAR_W, con_row * CHAR_H,
                                ' ', vga_colors[con_fg], vga_colors[con_bg]);
        }
    } else {
        fb_draw_char_bitmap(con_col * CHAR_W, con_row * CHAR_H,
                            c, vga_colors[con_fg], vga_colors[con_bg]);
        con_col++;
        if (con_col >= CONSOLE_COLS) {
            con_col = 0;
            con_row++;
        }
    }

    if (con_row >= CONSOLE_ROWS)
        console_scroll();
}

void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

void console_set_color(u8 fg, u8 bg) {
    con_fg = fg & 0x0F;
    con_bg = bg & 0x0F;
}

void console_clear(void) {
    if (fb_ready)
        fb_fill_rect(0, 0, CONSOLE_COLS * CHAR_W, CONSOLE_ROWS * CHAR_H,
                     vga_colors[con_bg]);
    con_col = 0;
    con_row = 0;
}

void console_init(void) {
    console_ready = true;
    console_clear();
}
