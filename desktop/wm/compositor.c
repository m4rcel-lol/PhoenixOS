/* compositor.c — AshDE software compositor */

#include "../../lib/libgui/include/gui.h"
#include <stdlib.h>
#include <string.h>

/* ── Back buffer ──────────────────────────────────────────────────────────── */

static u32 *back_buf   = NULL;
static u32 *front_buf  = NULL;  /* real framebuffer */
static u32  comp_w     = 0;
static u32  comp_h     = 0;
static u32  comp_pitch = 0;

/* Dirty rect tracking */
#define MAX_DIRTY  64
static Rect dirty_rects[MAX_DIRTY];
static int  dirty_count = 0;

/* ── comp_init ────────────────────────────────────────────────────────────── */

int comp_init(u32 *fb, u32 width, u32 height, u32 pitch) {
    front_buf  = fb;
    comp_w     = width;
    comp_h     = height;
    comp_pitch = pitch;
    back_buf   = (u32 *)malloc(width * height * sizeof(u32));
    if (!back_buf) return -1;
    memset(back_buf, 0, width * height * sizeof(u32));
    return 0;
}

/* ── Dirty rect management ────────────────────────────────────────────────── */

void comp_mark_dirty(s32 x, s32 y, s32 w, s32 h) {
    if (dirty_count >= MAX_DIRTY) return;
    dirty_rects[dirty_count++] = rect_make(x, y, w, h);
}

void comp_mark_all_dirty(void) {
    dirty_count = 1;
    dirty_rects[0] = rect_make(0, 0, (s32)comp_w, (s32)comp_h);
}

/* ── draw_desktop_background ──────────────────────────────────────────────── */

void draw_desktop_background(void) {
    /* Solid teal for now; could tile a bitmap wallpaper */
    for (u32 y = 0; y < comp_h; y++)
        for (u32 x = 0; x < comp_w; x++)
            back_buf[y * comp_w + x] = GUI_DESKTOP;
}

/* ── draw_window_decoration ───────────────────────────────────────────────── */

void draw_window_decoration(Window *w, bool active) {
    if (!w || !back_buf) return;

    int bw = 3;  /* border width */
    int th = 18; /* titlebar height */

    s32 ox = w->rect.x - bw;
    s32 oy = w->rect.y - th - bw;
    s32 ow = w->rect.w + bw * 2;
    s32 oh = w->rect.h + th + bw * 2;

    /* Background of frame */
    for (s32 row = oy; row < oy + oh; row++) {
        if (row < 0 || (u32)row >= comp_h) continue;
        for (s32 col = ox; col < ox + ow; col++) {
            if (col < 0 || (u32)col >= comp_w) continue;
            back_buf[row * comp_w + col] = GUI_BTN_FACE;
        }
    }

    /* Title bar */
    u32 tc = active ? GUI_TITLEBAR_ACTIVE : GUI_TITLEBAR_INACTIVE;
    for (s32 row = oy + bw; row < oy + bw + th; row++) {
        if (row < 0 || (u32)row >= comp_h) continue;
        for (s32 col = ox + bw; col < ox + ow - bw; col++) {
            if (col < 0 || (u32)col >= comp_w) continue;
            back_buf[row * comp_w + col] = tc;
        }
    }
}

/* ── Blit windows ─────────────────────────────────────────────────────────── */

void comp_blit_window(Window *w) {
    if (!w || !w->back_buf || !back_buf) return;
    for (s32 row = 0; row < w->rect.h; row++) {
        s32 dy = w->rect.y + row;
        if (dy < 0 || (u32)dy >= comp_h) continue;
        for (s32 col = 0; col < w->rect.w; col++) {
            s32 dx = w->rect.x + col;
            if (dx < 0 || (u32)dx >= comp_w) continue;
            back_buf[dy * comp_w + dx] = w->back_buf[row * w->rect.w + col];
        }
    }
}

/* ── present ──────────────────────────────────────────────────────────────── */

void present(void) {
    if (!front_buf || !back_buf) return;
    u32 stride = comp_pitch / 4;

    if (dirty_count == 0) return;

    for (int di = 0; di < dirty_count; di++) {
        Rect r = dirty_rects[di];
        /* Clip */
        if (r.x < 0) { r.w += r.x; r.x = 0; }
        if (r.y < 0) { r.h += r.y; r.y = 0; }
        if (r.x + r.w > (s32)comp_w) r.w = (s32)comp_w - r.x;
        if (r.y + r.h > (s32)comp_h) r.h = (s32)comp_h - r.y;
        if (r.w <= 0 || r.h <= 0) continue;

        for (s32 y = r.y; y < r.y + r.h; y++)
            for (s32 x = r.x; x < r.x + r.w; x++)
                front_buf[y * stride + x] = back_buf[y * comp_w + x];
    }
    dirty_count = 0;
}

/* ── Full composite frame ─────────────────────────────────────────────────── */

void comp_frame(Window **wins, int count, int focused) {
    draw_desktop_background();
    for (int i = 0; i < count; i++) {
        if (!wins[i]) continue;
        draw_window_decoration(wins[i], i == focused);
        comp_blit_window(wins[i]);
    }
    comp_mark_all_dirty();
    present();
}
