/* forge.c — Forge Control Panel for AshDE */

#include "../../lib/libgui/include/gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>

/* ── Applet list ──────────────────────────────────────────────────────────── */

typedef void (*AppletFn)(Window *w, Rect r);

typedef struct {
    const char *name;
    const char *icon_label;
    AppletFn    draw;
} Applet;

/* ── Display applet ───────────────────────────────────────────────────────── */

static void applet_display(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8,  "Display Settings",   GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 30, "Resolution: 640x480", GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 48, "Color Depth: 32bpp",  GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 70, "Background Color:",   GUI_TEXT, GUI_BTN_FACE);
    /* Color swatch */
    gui_draw_fill(w, rect_make(r.x + 8, r.y + 90, 60, 20), GUI_TEAL);
    gui_draw_bevel(w, rect_make(r.x + 8, r.y + 90, 60, 20), false);
}

/* ── Appearance applet ────────────────────────────────────────────────────── */

static const char *themes[] = {
    "Retro Gray", "Dark Ember", "Teal Classic", "Mono Classic", "High Contrast"
};
static int selected_theme = 0;

static void applet_appearance(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8, "Color Scheme:", GUI_TEXT, GUI_BTN_FACE);

    for (int i = 0; i < 5; i++) {
        Rect br = rect_make(r.x + 8, r.y + 28 + i * 22, r.w - 16, 18);
        gui_button_draw(w, br, themes[i], i == selected_theme, false);
    }
}

/* ── System info applet ───────────────────────────────────────────────────── */

static void applet_sysinfo(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);

    struct utsname un;
    uname(&un);

    char buf[256];
    snprintf(buf, sizeof(buf), "OS: PhoenixOS (EmberKernel)");
    gui_draw_text(w, r.x + 8, r.y + 8, buf, GUI_TEXT, GUI_BTN_FACE);

    snprintf(buf, sizeof(buf), "Kernel: %s %s", un.sysname, un.release);
    gui_draw_text(w, r.x + 8, r.y + 26, buf, GUI_TEXT, GUI_BTN_FACE);

    snprintf(buf, sizeof(buf), "Arch: %s", un.machine);
    gui_draw_text(w, r.x + 8, r.y + 44, buf, GUI_TEXT, GUI_BTN_FACE);

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        long totalMB = (long)(si.totalram / 1048576);
        long freeMB  = (long)(si.freeram  / 1048576);
        snprintf(buf, sizeof(buf), "Memory: %ld MB total, %ld MB free", totalMB, freeMB);
        gui_draw_text(w, r.x + 8, r.y + 62, buf, GUI_TEXT, GUI_BTN_FACE);

        long updays  = si.uptime / 86400;
        long uphours = (si.uptime % 86400) / 3600;
        long upmins  = (si.uptime % 3600)  / 60;
        snprintf(buf, sizeof(buf), "Uptime: %ldd %ldh %ldm", updays, uphours, upmins);
        gui_draw_text(w, r.x + 8, r.y + 80, buf, GUI_TEXT, GUI_BTN_FACE);
    }
}

/* ── Date/Time applet ─────────────────────────────────────────────────────── */

static void applet_datetime(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8, "Date & Time", GUI_TEXT, GUI_BTN_FACE);

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%A, %d %B %Y", tm);
    gui_draw_text(w, r.x + 8, r.y + 30, buf, GUI_TEXT, GUI_BTN_FACE);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    gui_draw_text(w, r.x + 8, r.y + 50, buf, GUI_TEXT, GUI_BTN_FACE);
}

/* ── Keyboard applet ──────────────────────────────────────────────────────── */

static void applet_keyboard(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8,  "Keyboard Settings",        GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 30, "Layout: US QWERTY",        GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 48, "Repeat Rate: Normal",      GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 66, "Repeat Delay: 500ms",      GUI_TEXT, GUI_BTN_FACE);
}

/* ── Mouse applet ─────────────────────────────────────────────────────────── */

static void applet_mouse(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8,  "Mouse Settings",         GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 30, "Speed: Normal",          GUI_TEXT, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 48, "Left-handed: No",        GUI_TEXT, GUI_BTN_FACE);
}

/* ── Users applet ─────────────────────────────────────────────────────────── */

static void applet_users(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    gui_draw_text(w, r.x + 8, r.y + 8, "User Accounts", GUI_TEXT, GUI_BTN_FACE);
    /* List users from /etc/passwd */
    FILE *f = fopen("/etc/passwd", "r");
    int y = 30;
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && y < r.h - 10) {
            if (line[0] == '#') continue;
            char *colon = strchr(line, ':');
            if (colon) *colon = '\0';
            gui_draw_text(w, r.x + 8, r.y + y, line, GUI_TEXT, GUI_BTN_FACE);
            y += 18;
        }
        fclose(f);
    }
}

/* ── Applet registry ──────────────────────────────────────────────────────── */

static Applet applets[] = {
    { "Display",    "DISP", applet_display    },
    { "Appearance", "APPR", applet_appearance },
    { "System Info","INFO", applet_sysinfo    },
    { "Date/Time",  "CLCK", applet_datetime   },
    { "Keyboard",   "KBRD", applet_keyboard   },
    { "Mouse",      "MOUS", applet_mouse      },
    { "Users",      "USER", applet_users      },
};
#define APPLET_COUNT 7

/* ── Forge window state ───────────────────────────────────────────────────── */

static int  active_applet = -1;
static Window *forge_win  = NULL;

/* ── Draw main grid ───────────────────────────────────────────────────────── */

#define ICON_W  72
#define ICON_H  72
#define GRID_X  8
#define GRID_Y  8
#define GRID_COLS 4

static void draw_grid(Window *w) {
    Rect title_r = rect_make(0, 0, w->rect.w, 20);
    gui_draw_fill(w, title_r, GUI_TITLEBAR_ACTIVE);
    gui_draw_text(w, 4, 2, "Forge — Control Panel", GUI_WHITE, GUI_TITLEBAR_ACTIVE);

    for (int i = 0; i < APPLET_COUNT; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        s32 ix  = GRID_X + col * (ICON_W + 8);
        s32 iy  = 24     + row * (ICON_H + 8);

        Rect ir = rect_make(ix, iy, ICON_W, ICON_H);
        gui_draw_fill(w, ir, GUI_BTN_FACE);
        gui_draw_bevel(w, ir, i != active_applet);

        /* Icon label (big text) */
        gui_draw_text(w, ix + 8,  iy + 12, applets[i].icon_label,
                      GUI_DARK_BLUE, GUI_BTN_FACE);
        /* Applet name below */
        gui_draw_text(w, ix + 2,  iy + ICON_H - 18, applets[i].name,
                      GUI_TEXT, GUI_BTN_FACE);
    }
}

/* ── Draw event ───────────────────────────────────────────────────────────── */

static void forge_draw(Window *w) {
    gui_draw_fill(w, rect_make(0, 0, w->rect.w, w->rect.h), GUI_BTN_FACE);

    if (active_applet < 0) {
        draw_grid(w);
        return;
    }

    /* Applet view */
    Rect back_btn = rect_make(4, 4, 60, 18);
    gui_button_draw(w, back_btn, "<< Back", false, false);

    gui_draw_fill(w,
        rect_make(0, 0, w->rect.w, 24), GUI_TITLEBAR_ACTIVE);
    gui_draw_text(w, 70, 4, applets[active_applet].name,
                  GUI_WHITE, GUI_TITLEBAR_ACTIVE);

    Rect content = rect_make(4, 28, w->rect.w - 8, w->rect.h - 36);
    gui_draw_fill(w, content, GUI_BTN_FACE);
    gui_draw_bevel(w, content, false);
    applets[active_applet].draw(w, rect_inset(content, 3));

    /* OK button */
    Rect ok_btn = rect_make(w->rect.w - 70, w->rect.h - 26, 64, 20);
    gui_button_draw(w, ok_btn, "OK", false, false);
}

static void forge_event(Window *w, Event *evt) {
    if (evt->type == EVT_MOUSE_DOWN) {
        s32 mx = evt->mouse.x, my = evt->mouse.y;

        if (active_applet < 0) {
            /* Check icon clicks */
            for (int i = 0; i < APPLET_COUNT; i++) {
                int col = i % GRID_COLS;
                int row = i / GRID_COLS;
                s32 ix  = GRID_X + col * (ICON_W + 8);
                s32 iy  = 24     + row * (ICON_H + 8);
                if (rect_contains(rect_make(ix, iy, ICON_W, ICON_H), mx, my)) {
                    active_applet = i;
                    gui_redraw_window(w);
                    return;
                }
            }
        } else {
            /* Back button */
            if (rect_contains(rect_make(4, 4, 60, 18), mx, my)) {
                active_applet = -1;
                gui_redraw_window(w);
                return;
            }
            /* OK button */
            s32 ok_x = w->rect.w - 70, ok_y = w->rect.h - 26;
            if (rect_contains(rect_make(ok_x, ok_y, 64, 20), mx, my)) {
                active_applet = -1;
                gui_redraw_window(w);
                return;
            }
        }
    }
    (void)w;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    u32 *fb = (u32 *)calloc(800 * 600, 4);
    gui_init(fb, 800, 600, 800 * 4);

    forge_win = gui_create_window("Forge — Control Panel",
                                   100, 80, 380, 340, WIN_RESIZABLE);
    if (!forge_win) return 1;

    forge_win->draw_callback  = forge_draw;
    forge_win->event_callback = forge_event;

    gui_run();
    return 0;
}
