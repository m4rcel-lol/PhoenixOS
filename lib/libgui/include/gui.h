#ifndef GUI_H
#define GUI_H

/* libgui — AshDE GUI library (retro Windows 1.0/3.0 style) */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── Basic types ──────────────────────────────────────────────────────────── */

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef int32_t  s32;
typedef uint8_t  bool;
#define true  1
#define false 0
#ifndef NULL
#define NULL ((void*)0)
#endif

/* ── Color constants (RGB packed: 0xRRGGBB) ──────────────────────────────── */

#define GUI_BLACK          0x000000
#define GUI_WHITE          0xFFFFFF
#define GUI_GRAY           0xC0C0C0
#define GUI_DARK_GRAY      0x808080
#define GUI_LIGHT_GRAY     0xDFDFDF
#define GUI_TEAL           0x008080
#define GUI_DARK_BLUE      0x000080
#define GUI_BLUE           0x0000AA
#define GUI_RED            0xAA0000
#define GUI_GREEN          0x00AA00
#define GUI_YELLOW         0xAAAA00
#define GUI_CYAN           0x00AAAA

/* Classic Windows 3.x palette */
#define GUI_BTN_FACE        GUI_GRAY
#define GUI_BTN_HIGHLIGHT   GUI_WHITE
#define GUI_BTN_SHADOW      GUI_DARK_GRAY
#define GUI_BTN_DARK_SHADOW 0x404040
#define GUI_TITLEBAR_ACTIVE GUI_DARK_BLUE
#define GUI_TITLEBAR_INACTIVE GUI_DARK_GRAY
#define GUI_DESKTOP         GUI_TEAL
#define GUI_MENU_BG         GUI_GRAY
#define GUI_MENU_HIGHLIGHT  GUI_DARK_BLUE
#define GUI_TEXT            GUI_BLACK
#define GUI_DISABLED_TEXT   GUI_DARK_GRAY

/* ── Rect ─────────────────────────────────────────────────────────────────── */

typedef struct {
    s32 x, y, w, h;
} Rect;

static inline bool rect_contains(Rect r, s32 x, s32 y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static inline Rect rect_make(s32 x, s32 y, s32 w, s32 h) {
    Rect r = {x, y, w, h}; return r;
}

static inline Rect rect_inset(Rect r, s32 n) {
    return rect_make(r.x + n, r.y + n, r.w - 2*n, r.h - 2*n);
}

/* ── Color struct ─────────────────────────────────────────────────────────── */

typedef struct { u8 r, g, b, a; } Color;

static inline Color color_from_rgb(u32 rgb) {
    Color c;
    c.r = (rgb >> 16) & 0xFF;
    c.g = (rgb >> 8)  & 0xFF;
    c.b =  rgb        & 0xFF;
    c.a = 255;
    return c;
}

/* ── Event types ──────────────────────────────────────────────────────────── */

typedef enum {
    EVT_NONE = 0,
    EVT_MOUSE_MOVE,
    EVT_MOUSE_DOWN,
    EVT_MOUSE_UP,
    EVT_KEY_DOWN,
    EVT_KEY_UP,
    EVT_CLOSE,
    EVT_RESIZE,
    EVT_FOCUS,
    EVT_BLUR,
    EVT_PAINT,
    EVT_TIMER,
    EVT_QUIT,
} EventType;

typedef struct {
    EventType type;
    union {
        struct { s32 x, y, button; } mouse;
        struct { u32 keycode; u8 modifiers; char ch; } key;
        struct { s32 w, h; } resize;
    };
    u32 window_id;
    u32 timestamp;
} Event;

/* ── Window flags ─────────────────────────────────────────────────────────── */

#define WIN_NONE        0x00
#define WIN_RESIZABLE   0x01
#define WIN_BORDERLESS  0x02
#define WIN_TOPMOST     0x04
#define WIN_MINIMIZED   0x08
#define WIN_MAXIMIZED   0x10
#define WIN_NO_TITLEBAR 0x20

/* ── Window ───────────────────────────────────────────────────────────────── */

typedef struct Window {
    u32  id;
    char title[64];
    Rect rect;
    u32  flags;
    u32 *back_buf;    /* off-screen pixel buffer */
    void (*draw_callback)(struct Window *w);
    void (*event_callback)(struct Window *w, Event *evt);
    void *userdata;
} Window;

/* ── Widget types ─────────────────────────────────────────────────────────── */

typedef enum {
    WIDGET_BUTTON = 0,
    WIDGET_LABEL,
    WIDGET_TEXTBOX,
    WIDGET_LISTBOX,
    WIDGET_SCROLLBAR,
    WIDGET_CHECKBOX,
    WIDGET_MENUBAR,
} WidgetType;

/* ── Text alignment ───────────────────────────────────────────────────────── */

#define ALIGN_LEFT   0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT  2

/* ── GUI API ──────────────────────────────────────────────────────────────── */

int     gui_init(u32 *fb, u32 width, u32 height, u32 pitch);
void    gui_run(void);
void    gui_quit(void);
int     gui_poll_event(Event *evt);
void    gui_redraw_all(void);

Window *gui_create_window(const char *title, s32 x, s32 y, s32 w, s32 h, u32 flags);
void    gui_destroy_window(Window *win);
void    gui_show_window(Window *win);
void    gui_hide_window(Window *win);
void    gui_raise_window(Window *win);
void    gui_set_window_title(Window *win, const char *title);
void    gui_redraw_window(Window *win);

/* ── Low-level draw (on Window backbuffer) ────────────────────────────────── */

void gui_draw_pixel(Window *w, s32 x, s32 y, u32 color);
void gui_draw_rect(Window *w, Rect r, u32 color);
void gui_draw_fill(Window *w, Rect r, u32 color);
void gui_draw_border(Window *w, Rect r, u32 tl, u32 br);
void gui_draw_bevel(Window *w, Rect r, bool raised);
void gui_draw_text(Window *w, s32 x, s32 y, const char *str, u32 color, u32 bg);
void gui_draw_titlebar(Window *w, Rect r, const char *title, bool active);

/* ── Widget drawing ───────────────────────────────────────────────────────── */

void gui_button_draw(Window *w, Rect r, const char *label, bool pressed, bool focused);
void gui_label_draw(Window *w, Rect r, const char *text, u32 color, u8 align);
void gui_textbox_draw(Window *w, Rect r, const char *text, int cursor_pos, bool focused);
void gui_listbox_draw(Window *w, Rect r, const char **items, int count, int selected);
void gui_scrollbar_draw(Window *w, Rect r, int total, int visible, int offset, bool vertical);
void gui_checkbox_draw(Window *w, Rect r, const char *label, bool checked, bool focused);
void gui_menubar_draw(Window *w, Rect r, const char **items, int count, int active_item);
int  gui_menubar_hit(Rect r, int count, s32 mx, s32 my);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
