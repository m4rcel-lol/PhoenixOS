/* panel.c — AshDE Taskbar / Panel */

#include "../../lib/libgui/include/gui.h"
#include "../../lib/libipc/include/ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PANEL_HEIGHT  28
#define CLOCK_WIDTH   60
#define LOGO_WIDTH    38
#define TASK_BTN_W    120
#define TASK_BTN_H    22

/* ── Task entry ───────────────────────────────────────────────────────────── */

#define MAX_TASKS  32

typedef struct {
    u32  win_id;
    char title[64];
    bool active;
    bool visible;
} TaskEntry;

static TaskEntry tasks[MAX_TASKS];
static int       task_count = 0;

/* IPC to WM */
static Channel wm_chan;
static bool    wm_connected = false;

/* Framebuffer */
static u32 *fb     = NULL;
static u32  screen_w = 640;
static u32  screen_h = 480;
static u32  fb_pitch = 640 * 4;

/* ── Draw panel ───────────────────────────────────────────────────────────── */

static void draw_panel(void) {
    if (!fb) return;
    u32 stride = fb_pitch / 4;
    s32 py = (s32)screen_h - PANEL_HEIGHT;

    /* Panel background */
    for (s32 y = py; y < (s32)screen_h; y++) {
        if (y < 0) continue;
        for (s32 x = 0; x < (s32)screen_w; x++)
            fb[y * stride + x] = GUI_BTN_FACE;
    }

    /* Top border line (shadow) */
    for (s32 x = 0; x < (s32)screen_w; x++)
        fb[py * stride + x] = GUI_BTN_SHADOW;
    if (py + 1 < (s32)screen_h)
        for (s32 x = 0; x < (s32)screen_w; x++)
            fb[(py + 1) * stride + x] = GUI_BTN_HIGHLIGHT;

    /* ── Phoenix logo / start button ── */
    s32 lx = 2, ly = py + 2;
    for (s32 y = ly; y < ly + PANEL_HEIGHT - 4; y++) {
        if (y < 0 || (u32)y >= screen_h) continue;
        for (s32 x = lx; x < lx + LOGO_WIDTH; x++) {
            if (x < 0 || (u32)x >= screen_w) continue;
            fb[y * stride + x] = GUI_BTN_FACE;
        }
    }
    /* Bevel for start button */
    u32 tl = GUI_BTN_HIGHLIGHT, br = GUI_BTN_SHADOW;
    for (s32 x = lx; x < lx + LOGO_WIDTH; x++) {
        fb[ly * stride + x] = tl;
        if (ly + PANEL_HEIGHT - 5 >= 0 && (u32)(ly + PANEL_HEIGHT - 5) < screen_h)
            fb[(ly + PANEL_HEIGHT - 5) * stride + x] = br;
    }
    for (s32 y = ly; y < ly + PANEL_HEIGHT - 4; y++) {
        fb[y * stride + lx] = tl;
        if (lx + LOGO_WIDTH - 1 >= 0 && (u32)(lx + LOGO_WIDTH - 1) < screen_w)
            fb[y * stride + lx + LOGO_WIDTH - 1] = br;
    }

    /* Draw "PX" text as logo placeholder */
    /* (In real implementation, draw a small phoenix icon) */

    /* ── Task buttons ── */
    s32 bx = LOGO_WIDTH + 4;
    for (int i = 0; i < task_count; i++) {
        if (!tasks[i].visible) continue;
        s32 by = py + 3;
        bool pressed = tasks[i].active;

        /* Button background */
        for (s32 y = by; y < by + TASK_BTN_H; y++) {
            if (y < 0 || (u32)y >= screen_h) continue;
            for (s32 x = bx; x < bx + TASK_BTN_W; x++) {
                if (x < 0 || (u32)x >= screen_w) continue;
                fb[y * stride + x] = GUI_BTN_FACE;
            }
        }
        /* Bevel */
        for (s32 x = bx; x < bx + TASK_BTN_W; x++) {
            u32 col1 = pressed ? GUI_BTN_SHADOW : GUI_BTN_HIGHLIGHT;
            u32 col2 = pressed ? GUI_BTN_HIGHLIGHT : GUI_BTN_SHADOW;
            if ((u32)by < screen_h)          fb[by * stride + x] = col1;
            if ((u32)(by + TASK_BTN_H - 1) < screen_h) fb[(by + TASK_BTN_H - 1) * stride + x] = col2;
        }
        for (s32 y = by; y < by + TASK_BTN_H; y++) {
            u32 col1 = pressed ? GUI_BTN_SHADOW : GUI_BTN_HIGHLIGHT;
            u32 col2 = pressed ? GUI_BTN_HIGHLIGHT : GUI_BTN_SHADOW;
            if ((u32)y < screen_h) {
                fb[y * stride + bx]                  = col1;
                fb[y * stride + bx + TASK_BTN_W - 1] = col2;
            }
        }

        bx += TASK_BTN_W + 2;
        if (bx + TASK_BTN_W > (s32)screen_w - CLOCK_WIDTH - 4) break;
    }

    /* ── Clock ── */
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char clock_str[16];
    strftime(clock_str, sizeof(clock_str), "%H:%M", tm);

    s32 clk_x = (s32)screen_w - CLOCK_WIDTH - 2;
    s32 clk_y = py + 2;
    for (s32 y = clk_y; y < clk_y + PANEL_HEIGHT - 4; y++) {
        if (y < 0 || (u32)y >= screen_h) continue;
        for (s32 x = clk_x; x < clk_x + CLOCK_WIDTH; x++) {
            if (x < 0 || (u32)x >= screen_w) continue;
            fb[y * stride + x] = GUI_BTN_FACE;
        }
    }
    /* Border around clock */
    for (s32 x = clk_x; x < clk_x + CLOCK_WIDTH; x++) {
        if ((u32)clk_y < screen_h)          fb[clk_y * stride + x] = GUI_BTN_SHADOW;
        if ((u32)(clk_y + PANEL_HEIGHT - 5) < screen_h)
            fb[(clk_y + PANEL_HEIGHT - 5) * stride + x] = GUI_BTN_HIGHLIGHT;
    }
}

/* ── IPC handler ──────────────────────────────────────────────────────────── */

static void connect_to_wm(void) {
    if (ipc_connect("wm", &wm_chan) == 0) wm_connected = true;
}

static void poll_ipc(void) {
    if (!wm_connected) return;
    Message msg;
    if (ipc_recv_timeout(&wm_chan, &msg, 0) > 0) {
        /* Handle window open/close messages */
        if (msg.type == MSG_WIN_CREATE && task_count < MAX_TASKS) {
            tasks[task_count].win_id  = msg.sender;
            tasks[task_count].visible = true;
            tasks[task_count].active  = false;
            memcpy(tasks[task_count].title, msg.data,
                   sizeof(tasks[0].title) - 1);
            task_count++;
        }
        if (msg.type == MSG_WIN_DESTROY) {
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].win_id == msg.sender) {
                    tasks[i].visible = false;
                }
            }
        }
    }
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("AshDE panel starting...\n");

    /* Try to open the framebuffer */
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd >= 0) {
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
        ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
        screen_w = vinfo.xres;
        screen_h = vinfo.yres;
        fb_pitch = finfo.line_length;
        fb = (u32 *)mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fbfd, 0);
        close(fbfd);
        if (fb == MAP_FAILED) fb = NULL;
    }

    if (!fb) {
        screen_w = 640; screen_h = 480; fb_pitch = screen_w * 4;
        fb = (u32 *)calloc(screen_w * screen_h, 4);
    }

    connect_to_wm();

    while (1) {
        draw_panel();
        poll_ipc();
        sleep(1);  /* update clock every second */
    }
    return 0;
}
