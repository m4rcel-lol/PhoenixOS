/* window.c — libgui window management and drawing for AshDE */

#include "include/gui.h"
#include <string.h>
#include <stdlib.h>

/* ── State ────────────────────────────────────────────────────────────────── */

static u32  *framebuffer = NULL;
static u32   fb_width    = 0;
static u32   fb_height   = 0;
static u32   fb_pitch    = 0;   /* bytes per row */

#define MAX_WINDOWS  64

static Window *windows[MAX_WINDOWS];
static int     win_count = 0;
static u32     next_win_id = 1;
static bool    running = false;

/* Keyboard/mouse state (read from /dev/input on real OS; unused in stub) */

/* ── 8x16 bitmap font embedded (same as kernel) ───────────────────────────── */

extern const u8 font8x16[96][16];  /* defined in a shared font file in a real build */

/* Fallback: use a simple built-in character renderer */

static void draw_char_to_buf(u32 *buf, u32 buf_w, s32 cx, s32 cy,
                              char c, u32 fg, u32 bg) {
    /* Simple 8x16 box font */
    static const u8 fallback[16] = {
        0x7e,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x7e,0,0,0,0,0,0
    };
    (void)c; (void)fallback;
    /* In a real build, reference the PSF font */
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            s32 px = cx + col, py = cy + row;
            if (px < 0 || py < 0 || (u32)px >= buf_w) continue;
            buf[(u32)py * buf_w + (u32)px] = bg;
        }
    }
    (void)fg;
}

/* ── gui_init ─────────────────────────────────────────────────────────────── */

int gui_init(u32 *fb, u32 width, u32 height, u32 pitch) {
    framebuffer = fb;
    fb_width    = width;
    fb_height   = height;
    fb_pitch    = pitch;
    running     = true;
    return 0;
}

/* ── gui_create_window ────────────────────────────────────────────────────── */

Window *gui_create_window(const char *title, s32 x, s32 y, s32 w, s32 h, u32 flags) {
    if (win_count >= MAX_WINDOWS) return NULL;
    Window *win = (Window *)calloc(1, sizeof(Window));
    if (!win) return NULL;

    win->id    = next_win_id++;
    win->rect  = rect_make(x, y, w, h);
    win->flags = flags;
    strncpy(win->title, title ? title : "", 63);

    win->back_buf = (u32 *)calloc((size_t)(w * h), sizeof(u32));
    if (!win->back_buf) { free(win); return NULL; }

    /* Fill with window background */
    for (int i = 0; i < w * h; i++) win->back_buf[i] = GUI_BTN_FACE;

    windows[win_count++] = win;
    return win;
}

/* ── gui_destroy_window ───────────────────────────────────────────────────── */

void gui_destroy_window(Window *win) {
    for (int i = 0; i < win_count; i++) {
        if (windows[i] == win) {
            free(win->back_buf);
            free(win);
            windows[i] = windows[--win_count];
            windows[win_count] = NULL;
            return;
        }
    }
}

/* ── gui_raise_window ─────────────────────────────────────────────────────── */

void gui_raise_window(Window *win) {
    for (int i = 0; i < win_count; i++) {
        if (windows[i] == win) {
            for (int j = i; j < win_count - 1; j++) windows[j] = windows[j+1];
            windows[win_count - 1] = win;
            return;
        }
    }
}

/* ── Pixel / rect drawing to a Window's back buffer ──────────────────────── */

void gui_draw_pixel(Window *w, s32 x, s32 y, u32 color) {
    if (!w->back_buf || x < 0 || y < 0 || x >= w->rect.w || y >= w->rect.h) return;
    w->back_buf[y * w->rect.w + x] = color;
}

void gui_draw_fill(Window *w, Rect r, u32 color) {
    for (s32 row = r.y; row < r.y + r.h; row++)
        for (s32 col = r.x; col < r.x + r.w; col++)
            gui_draw_pixel(w, col, row, color);
}

void gui_draw_rect(Window *w, Rect r, u32 color) {
    for (s32 x = r.x; x < r.x + r.w; x++) {
        gui_draw_pixel(w, x, r.y,           color);
        gui_draw_pixel(w, x, r.y + r.h - 1, color);
    }
    for (s32 y = r.y + 1; y < r.y + r.h - 1; y++) {
        gui_draw_pixel(w, r.x,           y, color);
        gui_draw_pixel(w, r.x + r.w - 1, y, color);
    }
}

/* ── Beveled 3D border (Windows 3.x style) ───────────────────────────────── */

void gui_draw_bevel(Window *w, Rect r, bool raised) {
    u32 tl  = raised ? GUI_BTN_HIGHLIGHT  : GUI_BTN_DARK_SHADOW;
    u32 br  = raised ? GUI_BTN_DARK_SHADOW : GUI_BTN_HIGHLIGHT;
    u32 tl2 = raised ? GUI_BTN_FACE       : GUI_BTN_SHADOW;
    u32 br2 = raised ? GUI_BTN_SHADOW     : GUI_BTN_FACE;

    /* Outer edge */
    for (s32 x = r.x; x < r.x + r.w; x++) {
        gui_draw_pixel(w, x, r.y,           tl);
        gui_draw_pixel(w, x, r.y + r.h - 1, br);
    }
    for (s32 y = r.y; y < r.y + r.h; y++) {
        gui_draw_pixel(w, r.x,           y, tl);
        gui_draw_pixel(w, r.x + r.w - 1, y, br);
    }
    /* Inner edge */
    Rect inner = rect_inset(r, 1);
    for (s32 x = inner.x; x < inner.x + inner.w; x++) {
        gui_draw_pixel(w, x, inner.y,              tl2);
        gui_draw_pixel(w, x, inner.y + inner.h - 1, br2);
    }
    for (s32 y = inner.y; y < inner.y + inner.h; y++) {
        gui_draw_pixel(w, inner.x,               y, tl2);
        gui_draw_pixel(w, inner.x + inner.w - 1, y, br2);
    }
}

/* ── Text rendering (8×8 simple, using ASCII bitmaps) ────────────────────── */

void gui_draw_text(Window *w, s32 x, s32 y, const char *str, u32 color, u32 bg) {
    s32 cx = x;
    for (const char *p = str; *p; p++) {
        if (*p == '\n') { cx = x; y += 16; continue; }
        if (*p == '\t') { cx += 8 * 4; continue; }
        draw_char_to_buf(w->back_buf, (u32)w->rect.w, cx, y, *p, color, bg);
        cx += 8;
    }
    (void)color;
}

/* ── Title bar ────────────────────────────────────────────────────────────── */

void gui_draw_titlebar(Window *w, Rect r, const char *title, bool active) {
    u32 bg = active ? GUI_TITLEBAR_ACTIVE : GUI_TITLEBAR_INACTIVE;
    gui_draw_fill(w, r, bg);

    /* Title text */
    s32 tx = r.x + 3;
    s32 ty = r.y + (r.h - 16) / 2;
    gui_draw_text(w, tx, ty, title, GUI_WHITE, bg);
}

/* ── Present window to framebuffer ───────────────────────────────────────── */

static void present_window(Window *win) {
    if (!win->back_buf || !framebuffer) return;
    s32 wx = win->rect.x, wy = win->rect.y;
    s32 ww = win->rect.w, wh = win->rect.h;
    u32 stride = fb_pitch / 4;

    for (s32 row = 0; row < wh; row++) {
        s32 dst_y = wy + row;
        if (dst_y < 0 || (u32)dst_y >= fb_height) continue;
        for (s32 col = 0; col < ww; col++) {
            s32 dst_x = wx + col;
            if (dst_x < 0 || (u32)dst_x >= fb_width) continue;
            framebuffer[(u32)dst_y * stride + (u32)dst_x] =
                win->back_buf[row * ww + col];
        }
    }
}

/* ── gui_redraw_all ───────────────────────────────────────────────────────── */

void gui_redraw_all(void) {
    /* Fill desktop background */
    if (framebuffer) {
        u32 stride = fb_pitch / 4;
        for (u32 y = 0; y < fb_height; y++)
            for (u32 x = 0; x < fb_width; x++)
                framebuffer[y * stride + x] = GUI_DESKTOP;
    }
    for (int i = 0; i < win_count; i++) {
        if (windows[i]->draw_callback) windows[i]->draw_callback(windows[i]);
        present_window(windows[i]);
    }
}

/* ── Minimal event poll ───────────────────────────────────────────────────── */

int gui_poll_event(Event *evt) {
    evt->type = EVT_NONE;
    /* In a real implementation, read from /dev/input/mouse and keyboard */
    return 0;
}

void gui_quit(void) { running = false; }

void gui_run(void) {
    Event evt;
    while (running) {
        gui_redraw_all();
        while (gui_poll_event(&evt) && evt.type != EVT_NONE) {
            /* Dispatch to focused window */
            if (win_count > 0) {
                Window *top = windows[win_count - 1];
                if (top->event_callback) top->event_callback(top, &evt);
            }
            if (evt.type == EVT_QUIT) running = false;
        }
    }
}

void gui_redraw_window(Window *win) {
    if (win->draw_callback) win->draw_callback(win);
    present_window(win);
}

void gui_set_window_title(Window *win, const char *title) {
    strncpy(win->title, title, 63);
}
