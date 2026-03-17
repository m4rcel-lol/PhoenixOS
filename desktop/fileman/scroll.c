/* scroll.c — Scroll File Manager for AshDE */

#include "../../lib/libgui/include/gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define MAX_FILES  1024
#define MAX_PATH   4096

typedef struct {
    char     name[256];
    off_t    size;
    mode_t   mode;
    time_t   mtime;
    bool     is_dir;
} FileEntry;

/* ── State ────────────────────────────────────────────────────────────────── */

static char       left_path[MAX_PATH]  = "/";
static char       right_path[MAX_PATH] = "/";
static FileEntry  left_files[MAX_FILES];
static FileEntry  right_files[MAX_FILES];
static int        left_count  = 0, right_count  = 0;
static int        left_sel    = 0, right_sel    = 0;
static int        left_scroll = 0, right_scroll = 0;
static bool       left_active = true;

static Window *main_win = NULL;

/* ── Read directory ───────────────────────────────────────────────────────── */

static int read_dir(const char *path, FileEntry *entries, int max) {
    DIR *d = opendir(path);
    if (!d) return 0;

    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && count < max) {
        if (strcmp(de->d_name, ".") == 0) continue;

        FileEntry *fe = &entries[count];
        strncpy(fe->name, de->d_name, 255);

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);

        struct stat st;
        if (stat(full, &st) == 0) {
            fe->size  = st.st_size;
            fe->mode  = st.st_mode;
            fe->mtime = st.st_mtime;
            fe->is_dir = S_ISDIR(st.st_mode);
        }
        count++;
    }
    closedir(d);

    /* Sort: directories first, then files, both alphabetical */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            bool swap = false;
            if (entries[i].is_dir == entries[j].is_dir)
                swap = strcmp(entries[i].name, entries[j].name) > 0;
            else if (!entries[i].is_dir && entries[j].is_dir)
                swap = true;
            if (swap) {
                FileEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
    return count;
}

/* ── Format size ──────────────────────────────────────────────────────────── */

static void fmt_size(off_t size, char *buf, int bufsz) {
    if (size < 1024)          snprintf(buf, bufsz, "%lld", (long long)size);
    else if (size < 1048576)  snprintf(buf, bufsz, "%lldk", (long long)(size/1024));
    else                      snprintf(buf, bufsz, "%lldM", (long long)(size/1048576));
}

/* ── Draw a file pane ─────────────────────────────────────────────────────── */

static void draw_pane(Window *w, Rect r, const char *path, FileEntry *files,
                      int count, int sel, int scroll, bool active) {
    gui_draw_fill(w, r, GUI_WHITE);
    gui_draw_bevel(w, r, false);

    /* Path bar */
    Rect path_r = rect_make(r.x + 2, r.y + 2, r.w - 4, 16);
    gui_draw_fill(w, path_r, active ? GUI_TITLEBAR_ACTIVE : GUI_BTN_FACE);
    gui_draw_text(w, path_r.x + 2, path_r.y, path,
                  active ? GUI_WHITE : GUI_TEXT,
                  active ? GUI_TITLEBAR_ACTIVE : GUI_BTN_FACE);

    /* File list */
    int item_h = 16;
    Rect list_r = rect_make(r.x + 2, r.y + 20, r.w - 4, r.h - 22);
    int visible = list_r.h / item_h;

    for (int i = 0; i < visible && (i + scroll) < count; i++) {
        int idx = i + scroll;
        FileEntry *fe = &files[idx];
        Rect ir = rect_make(list_r.x, list_r.y + i * item_h, list_r.w, item_h);

        u32 bg = (idx == sel && active) ? GUI_MENU_HIGHLIGHT :
                 (idx == sel)           ? GUI_LIGHT_GRAY     : GUI_WHITE;
        u32 fg = (idx == sel && active) ? GUI_WHITE : GUI_TEXT;
        gui_draw_fill(w, ir, bg);

        /* Icon indicator */
        char prefix = fe->is_dir ? '[' : ' ';
        char name[260];
        if (fe->is_dir) snprintf(name, sizeof(name), "[%s]", fe->name);
        else            snprintf(name, sizeof(name), " %s", fe->name);

        gui_draw_text(w, ir.x + 2, ir.y, name, fg, bg);

        /* Size on right */
        if (!fe->is_dir) {
            char sz[16];
            fmt_size(fe->size, sz, sizeof(sz));
            int len = (int)strlen(sz);
            gui_draw_text(w, ir.x + ir.w - len * 8 - 4, ir.y, sz, fg, bg);
        }
        (void)prefix;
    }
}

/* ── Toolbar ──────────────────────────────────────────────────────────────── */

static const char *toolbar_btns[] = {"F5 Copy", "F6 Move", "F7 MkDir", "F8 Delete", "F10 Quit"};
#define TOOLBAR_COUNT 5

static void draw_toolbar(Window *w, Rect r) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    /* Top border */
    for (s32 x = r.x; x < r.x + r.w; x++)
        gui_draw_pixel(w, x, r.y, GUI_BTN_SHADOW);

    s32 bx = r.x + 2;
    for (int i = 0; i < TOOLBAR_COUNT; i++) {
        int len = (int)strlen(toolbar_btns[i]);
        s32 bw  = len * 8 + 12;
        Rect br = rect_make(bx, r.y + 2, bw, r.h - 4);
        gui_button_draw(w, br, toolbar_btns[i], false, false);
        bx += bw + 4;
    }
}

/* ── Draw callback ────────────────────────────────────────────────────────── */

static void scroll_draw(Window *w) {
    s32 ww = w->rect.w, wh = w->rect.h;
    gui_draw_fill(w, rect_make(0, 0, ww, wh), GUI_BTN_FACE);

    /* Toolbar at bottom */
    int toolbar_h = 24;
    Rect tb_r = rect_make(0, wh - toolbar_h, ww, toolbar_h);
    draw_toolbar(w, tb_r);

    /* Two equal panes */
    int half = (ww - 6) / 2;
    Rect left_r  = rect_make(2,         1, half,     wh - toolbar_h - 2);
    Rect right_r = rect_make(2 + half + 2, 1, half,  wh - toolbar_h - 2);

    draw_pane(w, left_r,  left_path,  left_files,  left_count,  left_sel,  left_scroll,  left_active);
    draw_pane(w, right_r, right_path, right_files, right_count, right_sel, right_scroll, !left_active);

    /* Vertical separator */
    for (s32 y = 1; y < wh - toolbar_h - 1; y++)
        gui_draw_pixel(w, 2 + half, y, GUI_BTN_SHADOW);
}

/* ── Event callback ───────────────────────────────────────────────────────── */

static void enter_selected(void) {
    FileEntry *files  = left_active ? left_files  : right_files;
    int        count  = left_active ? left_count  : right_count;
    int        sel    = left_active ? left_sel     : right_sel;
    char       *path  = left_active ? left_path    : right_path;

    if (sel < 0 || sel >= count) return;
    FileEntry *fe = &files[sel];

    if (fe->is_dir) {
        if (strcmp(fe->name, "..") == 0) {
            /* Go up */
            char *last_slash = strrchr(path, '/');
            if (last_slash && last_slash != path) *last_slash = '\0';
            else if (last_slash == path && path[1]) path[1] = '\0';
        } else {
            int plen = strlen(path);
            if (path[plen-1] != '/') strncat(path, "/", MAX_PATH - plen - 1);
            strncat(path, fe->name, MAX_PATH - strlen(path) - 1);
        }
        if (left_active) {
            left_count  = read_dir(left_path,  left_files,  MAX_FILES);
            left_sel    = 0;
            left_scroll = 0;
        } else {
            right_count  = read_dir(right_path,  right_files, MAX_FILES);
            right_sel    = 0;
            right_scroll = 0;
        }
    }
}

static void scroll_event(Window *w, Event *evt) {
    int *sel    = left_active ? &left_sel    : &right_sel;
    int *scroll = left_active ? &left_scroll : &right_scroll;
    int  count  = left_active ? left_count   : right_count;

    switch (evt->type) {
    case EVT_KEY_DOWN:
        switch (evt->key.keycode) {
        case 0x51:  /* Down */
            if (*sel < count - 1) (*sel)++;
            break;
        case 0x52:  /* Up */
            if (*sel > 0) (*sel)--;
            break;
        case '\n':  case '\r':
            enter_selected();
            break;
        case '\t':  /* Tab: switch pane */
            left_active = !left_active;
            break;
        }
        gui_redraw_window(w);
        break;
    default: break;
    }
    (void)scroll;
}

/* ── Main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    if (argc > 1) strncpy(left_path, argv[1], MAX_PATH - 1);
    else          getcwd(left_path, MAX_PATH);
    strncpy(right_path, left_path, MAX_PATH - 1);

    left_count  = read_dir(left_path,  left_files,  MAX_FILES);
    right_count = read_dir(right_path, right_files, MAX_FILES);

    u32 *fb = (u32 *)calloc(640 * 480, 4);
    gui_init(fb, 640, 480, 640 * 4);

    main_win = gui_create_window("Scroll — File Manager", 20, 20, 600, 440, WIN_RESIZABLE);
    if (!main_win) return 1;

    main_win->draw_callback  = scroll_draw;
    main_win->event_callback = scroll_event;

    gui_run();
    return 0;
}
