/* main.c — AshDE Window Manager */

#include "../../lib/libgui/include/gui.h"
#include "../../lib/libipc/include/ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <time.h>

/* ── WM state ─────────────────────────────────────────────────────────────── */

#define MAX_WM_WINDOWS  64
#define TITLEBAR_H      18
#define BORDER_W        3
#define BTN_W           14
#define BTN_H           14

typedef struct {
    Window     *win;
    bool        visible;
    bool        dragging;
    s32         drag_ox, drag_oy;  /* offset from mouse to window origin */
} WMWindow;

static WMWindow wm_windows[MAX_WM_WINDOWS];
static int      wm_win_count = 0;
static int      focused_idx  = -1;

/* Mouse state */
static s32 mouse_x = 320, mouse_y = 240;
static bool btn_left = false, btn_left_prev = false;

/* IPC server */
static int ipc_fd = -1;

/* ── Framebuffer init ─────────────────────────────────────────────────────── */

static u32 *fb_mem    = NULL;
static u32  fb_width  = 640;
static u32  fb_height = 480;
static u32  fb_pitch  = 640 * 4;

static int open_framebuffer(void) {
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "wm: cannot open /dev/fb0, using simulated mode\n");
        fb_width  = 640;
        fb_height = 480;
        fb_pitch  = fb_width * 4;
        fb_mem    = (u32 *)malloc(fb_width * fb_height * 4);
        return 0;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fd, FBIOGET_FSCREENINFO, &finfo);

    fb_width  = vinfo.xres;
    fb_height = vinfo.yres;
    fb_pitch  = finfo.line_length;

    fb_mem = (u32 *)mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);
    if (fb_mem == MAP_FAILED) { close(fd); return -1; }
    close(fd);
    return 0;
}

/* ── Draw desktop background ──────────────────────────────────────────────── */

static void draw_desktop(void) {
    u32 stride = fb_pitch / 4;
    /* Teal desktop */
    for (u32 y = 0; y < fb_height; y++)
        for (u32 x = 0; x < fb_width; x++)
            fb_mem[y * stride + x] = GUI_DESKTOP;
}

/* ── Draw window decoration ───────────────────────────────────────────────── */

static void draw_decoration(WMWindow *wmw, bool active) {
    Window *w = wmw->win;
    if (!w) return;

    /* Outer border (thick, gray) */
    s32 ox = w->rect.x - BORDER_W;
    s32 oy = w->rect.y - TITLEBAR_H - BORDER_W;
    s32 ow = w->rect.w + BORDER_W * 2;
    s32 oh = w->rect.h + TITLEBAR_H + BORDER_W * 2;

    u32 stride = fb_pitch / 4;
    u32 bg = GUI_BTN_FACE;

    /* Draw frame onto actual framebuffer */
    for (s32 y = oy; y < oy + oh; y++) {
        if (y < 0 || (u32)y >= fb_height) continue;
        for (s32 x = ox; x < ox + ow; x++) {
            if (x < 0 || (u32)x >= fb_width) continue;
            fb_mem[y * stride + x] = bg;
        }
    }

    /* Bevel the outer frame */
    /* Top/left light */
    for (s32 i = 0; i < BORDER_W; i++) {
        for (s32 x = ox + i; x < ox + ow - i; x++) {
            if ((u32)(oy + i) < fb_height && x >= 0 && (u32)x < fb_width)
                fb_mem[(oy + i) * stride + x] =
                    (i == 0) ? GUI_BTN_HIGHLIGHT : GUI_BTN_FACE;
        }
        for (s32 y = oy + i; y < oy + oh - i; y++) {
            if (y >= 0 && (u32)y < fb_height && ox + i >= 0 && (u32)(ox + i) < fb_width)
                fb_mem[y * stride + ox + i] =
                    (i == 0) ? GUI_BTN_HIGHLIGHT : GUI_BTN_FACE;
        }
    }

    /* Title bar */
    s32 tx = ox + BORDER_W;
    s32 ty = oy + BORDER_W;
    s32 tw = ow - BORDER_W * 2;
    s32 th = TITLEBAR_H;
    u32 tb_color = active ? GUI_TITLEBAR_ACTIVE : GUI_TITLEBAR_INACTIVE;

    for (s32 y = ty; y < ty + th; y++) {
        if (y < 0 || (u32)y >= fb_height) continue;
        for (s32 x = tx; x < tx + tw; x++) {
            if (x < 0 || (u32)x >= fb_width) continue;
            fb_mem[y * stride + x] = tb_color;
        }
    }

    /* Close button (simple square) */
    s32 cbx = tx + tw - BTN_W - 2;
    s32 cby = ty + (th - BTN_H) / 2;
    for (s32 y = cby; y < cby + BTN_H; y++) {
        if (y < 0 || (u32)y >= fb_height) continue;
        for (s32 x = cbx; x < cbx + BTN_W; x++) {
            if (x < 0 || (u32)x >= fb_width) continue;
            fb_mem[y * stride + x] = GUI_BTN_FACE;
        }
    }
    /* X mark */
    for (s32 i = 2; i < BTN_W - 2; i++) {
        s32 xi = cbx + i, yi1 = cby + i, yi2 = cby + BTN_H - 1 - i;
        if ((u32)xi < fb_width) {
            if ((u32)yi1 < fb_height) fb_mem[yi1 * stride + xi] = GUI_TEXT;
            if ((u32)yi2 < fb_height) fb_mem[yi2 * stride + xi] = GUI_TEXT;
        }
    }
}

/* ── Composite windows ────────────────────────────────────────────────────── */

static void composite(void) {
    draw_desktop();

    for (int i = 0; i < wm_win_count; i++) {
        WMWindow *wmw = &wm_windows[i];
        if (!wmw->visible || !wmw->win) continue;

        bool active = (i == focused_idx);
        draw_decoration(wmw, active);

        /* Blit window backbuffer */
        Window *w = wmw->win;
        u32 stride = fb_pitch / 4;
        for (s32 row = 0; row < w->rect.h; row++) {
            s32 dy = w->rect.y + row;
            if (dy < 0 || (u32)dy >= fb_height) continue;
            for (s32 col = 0; col < w->rect.w; col++) {
                s32 dx = w->rect.x + col;
                if (dx < 0 || (u32)dx >= fb_width) continue;
                fb_mem[dy * stride + dx] = w->back_buf[row * w->rect.w + col];
            }
        }
    }

    /* Draw mouse cursor */
    for (s32 i = 0; i < 12; i++) {
        s32 cx = mouse_x + i, cy = mouse_y + i;
        if ((u32)mouse_x < fb_width && (u32)(mouse_y + i) < fb_height)
            fb_mem[(mouse_y + i) * (fb_pitch/4) + mouse_x] = GUI_BLACK;
        if ((u32)cx < fb_width && (u32)mouse_y < fb_height && i < 8)
            fb_mem[mouse_y * (fb_pitch/4) + cx] = GUI_BLACK;
    }
}

/* ── IPC: handle client requests ─────────────────────────────────────────── */

static void handle_ipc(void) {
    /* In a real implementation, accept and handle MSG_WIN_CREATE etc. */
}

/* ── Main loop ────────────────────────────────────────────────────────────── */

int main(void) {
    printf("AshDE WM starting...\n");

    if (open_framebuffer() != 0) {
        fprintf(stderr, "wm: framebuffer init failed\n");
        return 1;
    }

    gui_init(fb_mem, fb_width, fb_height, fb_pitch);

    /* Create a demo window to show the WM is working */
    Window *demo = gui_create_window("PhoenixOS Desktop", 50, 50, 400, 300, 0);
    if (demo) {
        gui_draw_fill(demo, rect_make(0, 0, 400, 300), GUI_BTN_FACE);
        gui_draw_text(demo, 10, 10, "Welcome to PhoenixOS!", GUI_TEXT, GUI_BTN_FACE);
        gui_draw_text(demo, 10, 30, "AshDE Window Manager", GUI_TEXT, GUI_BTN_FACE);
        gui_draw_text(demo, 10, 50, "EmberKernel running", GUI_TEXT, GUI_BTN_FACE);

        /* Add to WM */
        wm_windows[0].win     = demo;
        wm_windows[0].visible = true;
        wm_win_count = 1;
        focused_idx  = 0;
    }

    ipc_fd = ipc_listen("wm");

    /* Main event loop */
    while (1) {
        composite();
        handle_ipc();
        usleep(16000);  /* ~60 fps */
    }

    return 0;
}
