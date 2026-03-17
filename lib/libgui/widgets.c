/* widgets.c — libgui widget drawing for AshDE */

#include "include/gui.h"
#include <string.h>

/* ── Button ───────────────────────────────────────────────────────────────── */

void gui_button_draw(Window *w, Rect r, const char *label, bool pressed, bool focused) {
    /* Fill face */
    gui_draw_fill(w, r, GUI_BTN_FACE);
    /* Beveled border */
    gui_draw_bevel(w, r, !pressed);

    /* Draw focus rectangle (dotted inner border) */
    if (focused) {
        Rect fr = rect_inset(r, 3);
        for (s32 x = fr.x; x < fr.x + fr.w; x += 2) {
            gui_draw_pixel(w, x, fr.y,              GUI_TEXT);
            gui_draw_pixel(w, x, fr.y + fr.h - 1,   GUI_TEXT);
        }
        for (s32 y = fr.y; y < fr.y + fr.h; y += 2) {
            gui_draw_pixel(w, fr.x,             y, GUI_TEXT);
            gui_draw_pixel(w, fr.x + fr.w - 1,  y, GUI_TEXT);
        }
    }

    /* Center text */
    if (label && *label) {
        int len = (int)strlen(label);
        s32 tx = r.x + (r.w - len * 8) / 2 + (pressed ? 1 : 0);
        s32 ty = r.y + (r.h - 16)      / 2 + (pressed ? 1 : 0);
        gui_draw_text(w, tx, ty, label, GUI_TEXT, GUI_BTN_FACE);
    }
}

/* ── Label ────────────────────────────────────────────────────────────────── */

void gui_label_draw(Window *w, Rect r, const char *text, u32 color, u8 align) {
    gui_draw_fill(w, r, GUI_BTN_FACE);
    if (!text || !*text) return;
    int len = (int)strlen(text);
    s32 tx;
    if (align == ALIGN_CENTER) tx = r.x + (r.w - len * 8) / 2;
    else if (align == ALIGN_RIGHT) tx = r.x + r.w - len * 8 - 2;
    else tx = r.x + 2;
    s32 ty = r.y + (r.h - 16) / 2;
    gui_draw_text(w, tx, ty, text, color, GUI_BTN_FACE);
}

/* ── Textbox ──────────────────────────────────────────────────────────────── */

void gui_textbox_draw(Window *w, Rect r, const char *text, int cursor_pos, bool focused) {
    /* Recessed border */
    gui_draw_fill(w, r, GUI_WHITE);
    gui_draw_bevel(w, r, false);  /* inset */

    Rect inner = rect_inset(r, 2);
    gui_draw_fill(w, inner, GUI_WHITE);

    if (text && *text) {
        gui_draw_text(w, inner.x + 1, inner.y + (inner.h - 16) / 2,
                      text, GUI_TEXT, GUI_WHITE);
    }

    /* Cursor */
    if (focused) {
        int len = text ? (int)strlen(text) : 0;
        if (cursor_pos > len) cursor_pos = len;
        s32 cx = inner.x + 1 + cursor_pos * 8;
        for (s32 y = inner.y + 2; y < inner.y + inner.h - 2; y++)
            gui_draw_pixel(w, cx, y, GUI_TEXT);
    }
}

/* ── Listbox ──────────────────────────────────────────────────────────────── */

void gui_listbox_draw(Window *w, Rect r, const char **items, int count, int selected) {
    gui_draw_fill(w, r, GUI_WHITE);
    gui_draw_bevel(w, r, false);

    Rect inner = rect_inset(r, 2);
    s32 item_h = 18;
    int visible = inner.h / item_h;

    for (int i = 0; i < count && i < visible; i++) {
        Rect item_r = rect_make(inner.x, inner.y + i * item_h, inner.w, item_h);
        u32  bg = (i == selected) ? GUI_MENU_HIGHLIGHT : GUI_WHITE;
        u32  fg = (i == selected) ? GUI_WHITE          : GUI_TEXT;
        gui_draw_fill(w, item_r, bg);
        if (items[i])
            gui_draw_text(w, item_r.x + 2, item_r.y + 1, items[i], fg, bg);
    }
}

/* ── Scrollbar ────────────────────────────────────────────────────────────── */

void gui_scrollbar_draw(Window *w, Rect r, int total, int visible_count,
                        int offset, bool vertical) {
    gui_draw_fill(w, r, GUI_LIGHT_GRAY);

    /* Arrow buttons */
    int btn_sz = vertical ? r.w : r.h;
    Rect up_btn, dn_btn;
    if (vertical) {
        up_btn = rect_make(r.x, r.y,             btn_sz, btn_sz);
        dn_btn = rect_make(r.x, r.y + r.h - btn_sz, btn_sz, btn_sz);
    } else {
        up_btn = rect_make(r.x,             r.y, btn_sz, btn_sz);
        dn_btn = rect_make(r.x + r.w - btn_sz, r.y, btn_sz, btn_sz);
    }
    gui_draw_fill(w, up_btn, GUI_BTN_FACE);
    gui_draw_bevel(w, up_btn, true);
    gui_draw_fill(w, dn_btn, GUI_BTN_FACE);
    gui_draw_bevel(w, dn_btn, true);

    /* Thumb */
    if (total > visible_count && total > 0) {
        int track_len  = (vertical ? r.h : r.w) - btn_sz * 2;
        int thumb_len  = track_len * visible_count / total;
        if (thumb_len < 8) thumb_len = 8;
        int thumb_pos  = btn_sz + track_len * offset / total;
        Rect thumb;
        if (vertical)
            thumb = rect_make(r.x + 1, r.y + thumb_pos, r.w - 2, thumb_len);
        else
            thumb = rect_make(r.x + thumb_pos, r.y + 1, thumb_len, r.h - 2);
        gui_draw_fill(w, thumb, GUI_BTN_FACE);
        gui_draw_bevel(w, thumb, true);
    }
}

/* ── Checkbox ─────────────────────────────────────────────────────────────── */

void gui_checkbox_draw(Window *w, Rect r, const char *label, bool checked, bool focused) {
    s32 box_sz = 13;
    Rect box = rect_make(r.x + 2, r.y + (r.h - box_sz) / 2, box_sz, box_sz);

    gui_draw_fill(w, box, GUI_WHITE);
    gui_draw_bevel(w, box, false);

    if (checked) {
        /* Draw checkmark */
        for (s32 i = 0; i < 3; i++) {
            gui_draw_pixel(w, box.x + 2 + i, box.y + 5 + i, GUI_TEXT);
            gui_draw_pixel(w, box.x + 2 + i, box.y + 6 + i, GUI_TEXT);
        }
        for (s32 i = 0; i < 5; i++) {
            gui_draw_pixel(w, box.x + 5 + i, box.y + 7 - i, GUI_TEXT);
            gui_draw_pixel(w, box.x + 5 + i, box.y + 8 - i, GUI_TEXT);
        }
    }

    if (focused) {
        Rect fr = rect_inset(r, 1);
        for (s32 x = fr.x; x < fr.x + fr.w; x += 2) {
            gui_draw_pixel(w, x, fr.y,             GUI_TEXT);
            gui_draw_pixel(w, x, fr.y + fr.h - 1,  GUI_TEXT);
        }
    }

    s32 tx = box.x + box_sz + 4;
    s32 ty = r.y + (r.h - 16) / 2;
    if (label) gui_draw_text(w, tx, ty, label, GUI_TEXT, GUI_BTN_FACE);
}

/* ── Menubar ──────────────────────────────────────────────────────────────── */

void gui_menubar_draw(Window *w, Rect r, const char **items, int count, int active_item) {
    gui_draw_fill(w, r, GUI_BTN_FACE);

    /* Bottom border line */
    for (s32 x = r.x; x < r.x + r.w; x++)
        gui_draw_pixel(w, x, r.y + r.h - 1, GUI_BTN_SHADOW);

    s32 cx = r.x + 2;
    for (int i = 0; i < count; i++) {
        if (!items[i]) continue;
        int len = (int)strlen(items[i]);
        s32 item_w = len * 8 + 12;
        Rect ir = rect_make(cx, r.y + 1, item_w, r.h - 2);

        if (i == active_item) {
            gui_draw_fill(w, ir, GUI_MENU_HIGHLIGHT);
            gui_draw_text(w, cx + 6, r.y + (r.h - 16) / 2,
                          items[i], GUI_WHITE, GUI_MENU_HIGHLIGHT);
        } else {
            gui_draw_text(w, cx + 6, r.y + (r.h - 16) / 2,
                          items[i], GUI_TEXT, GUI_BTN_FACE);
        }
        cx += item_w;
    }
}

int gui_menubar_hit(Rect r, int count, s32 mx, s32 my) {
    (void)count;
    if (!rect_contains(r, mx, my)) return -1;
    /* Simplified: return index based on x position */
    s32 cx = r.x + 2;
    for (int i = 0; i < count; i++) {
        s32 item_w = 60;  /* approx */
        if (mx >= cx && mx < cx + item_w) return i;
        cx += item_w;
    }
    return -1;
}
