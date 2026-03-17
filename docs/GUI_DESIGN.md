# AshDE GUI Design Specification

## Philosophy

AshDE (Ash Desktop Environment) is deliberately **retro**. It draws heavy
inspiration from **Windows 1.0 / 3.0 / 3.11** and early RISC OS, not from
modern flat-design or Material interfaces.

The goals are:
- **Clarity**: every control is immediately recognizable as a button, field,
  or list; no ambiguity about what is clickable
- **Lightweight**: software rendering on a linear framebuffer; no GPU, no
  compositing tricks, no alpha blending in the critical path
- **Retro charm**: beveled borders, gray window backgrounds, two-color title
  bars, square buttons — the aesthetic of early '90s GUIs

---

## Color Palette

| Role | Hex | Description |
|------|-----|-------------|
| Desktop | `#008080` | Classic teal desktop background |
| Window background | `#c0c0c0` | Standard "Windows gray" |
| Titlebar (active) | `#000080` | Dark navy blue |
| Titlebar (inactive) | `#808080` | Medium gray |
| Titlebar text | `#ffffff` | White |
| Button face | `#c0c0c0` | Same as window bg |
| Button highlight | `#ffffff` | Top-left edge of 3D bevel |
| Button shadow | `#808080` | Bottom-right edge of 3D bevel |
| Button dark shadow | `#404040` | Outer bottom-right edge |
| Menu background | `#c0c0c0` | |
| Menu highlight | `#000080` | Selected item background |
| Menu text | `#000000` | Normal text |
| Menu highlight text | `#ffffff` | Selected item text |
| Text | `#000000` | |
| Disabled text | `#808080` | |
| White | `#ffffff` | Textbox/listbox backgrounds |

These constants are defined in `lib/libgui/include/gui.h` as `GUI_*` macros.

---

## Window Decorations

### Anatomy of a Window

```
┌────────────────────────────────────────┐  ← thick gray border (3px)
│ ┌──────────────────────────────────┬──┐│  ← title bar (navy blue, 18px tall)
│ │  Window Title                    │×/││    close button (square, 14×14)
│ └──────────────────────────────────┴──┘│
│ ┌──────────────────────────────────────┐│  ← client area (gray)
│ │                                      ││
│ │   Window content drawn here          ││
│ │                                      ││
│ └──────────────────────────────────────┘│
└────────────────────────────────────────┘
```

- **Border**: 3 px thick, beveled (highlight on top/left, shadow on bottom/right)
- **Title bar**: 18 px, filled with `GUI_TITLEBAR_ACTIVE` (#000080) when focused,
  `GUI_TITLEBAR_INACTIVE` (#808080) when not
- **Title text**: white, left-aligned, 3 px from left edge
- **Close button**: 14×14 square with `×` drawn in black pixels; raised bevel
- **Minimize / Maximize** (planned Phase 5): identical square buttons to the
  left of close

### 3D Bevel Style

AshDE uses the classic 4-edge bevel:

```
highlight  (top + left outer edge)
face       (top + left inner edge)
shadow     (bottom + right inner edge)
dark-shadow(bottom + right outer edge)
```

Raised controls (buttons, window borders): `highlight` on top-left, `dark-shadow`
on bottom-right.

Recessed controls (textboxes, list boxes): inverted — `dark-shadow` on top-left.

---

## Widget Style Guide

### Buttons

- Filled with `GUI_BTN_FACE`; 3D bevel applied
- Label centered in button area
- When pressed: bevel inverted + label offset 1 px right+down
- When focused: dotted rectangle 2 px inside the button edges
- Minimum size: 75 × 23 px

### Text Boxes

- White fill (`GUI_WHITE`)
- Recessed (inset) bevel
- Cursor: 1-pixel vertical bar at caret position
- Focused: cursor blinks at 500 ms interval

### List Boxes

- White fill with inset bevel
- Selected item: `GUI_MENU_HIGHLIGHT` background, white text
- Unselected items: white background, black text
- 18 px per row

### Scrollbars

- 16 px wide (vertical) or 16 px tall (horizontal)
- Square arrow buttons at each end (beveled, same as regular buttons)
- Thumb: `GUI_BTN_FACE` with raised bevel
- Track background: `GUI_LIGHT_GRAY` (stippled in classic Windows, solid here)

### Checkboxes

- 13×13 px recessed square, white fill
- Check mark drawn in black when checked
- Label to the right, 4 px gap

### Menus

- Background: `GUI_BTN_FACE` (gray)
- Selected item: `GUI_MENU_HIGHLIGHT` fill + white text
- Separator: 1 px dark line
- Menu bar: gray strip 18 px tall; items spaced 12 px padding each side

---

## Fonts

PhoenixOS uses a single **8×16 pixel bitmap font** throughout the system.

- Format: PSF2 (PC Screen Font)
- Located: `/assets/fonts/system8x16.psf`
- Encoding: ASCII 32–127 + Latin-1 128–255
- No anti-aliasing (1-bit per pixel per row)

All text rendering passes through `gui_draw_text()` in `lib/libgui/window.c`,
which reads glyph bitmaps and plots pixels directly into the window back-buffer.

---

## Window Management

- Windows can be **moved** by dragging the title bar
- Windows can be **resized** by dragging the border (Phase 5)
- **Z-order**: click any part of a window to raise it to the top
- **Close**: click the `×` button; sends `EVT_CLOSE` to the application
- No minimize/maximize animation — instant show/hide
- Maximum windows: 64 (software limit)

---

## Desktop Layout

```
┌──────────────────────────────────────────────────────────────────┐
│  (teal #008080 background)                                       │
│                                                                  │
│   ┌───────────────┐    ┌──────────────────────────────────┐     │
│   │ Program Mgr   │    │  File Manager (Scroll)           │     │
│   │               │    │  [Left pane]  │ [Right pane]     │     │
│   └───────────────┘    └──────────────────────────────────┘     │
│                                                                  │
│                                                                  │
├──────────────────────────────────────────────────────────────────┤
│ [PX] │ [Program Manager] │ [Scroll] │               │  14:37   │
└──────────────────────────────────────────────────────────────────┘
  ↑                                                         ↑
Start/logo    Task buttons                              Clock (HH:MM)
```

The **Panel** is always 28 px tall at the bottom; it is drawn by
`desktop/panel/panel.c` directly to the framebuffer after the WM finishes
compositing.

---

## Program Manager (Phase 5)

Modeled after Windows 3.0 Program Manager:

```
┌─────────────────────────────────────────────────────┐
│  Program Manager                              [─][□][×]│
├──── File  Window  Help ─────────────────────────────┤
│ ┌─── Main ────────────────────────────────────────┐ │
│ │  [SCRLL]   [FORGE]   [TERM]   [ABOUT]           │ │
│ │  Scroll   Control   Terminal   About              │ │
│ └─────────────────────────────────────────────────┘ │
│ ┌─── Games ───────────────────────────────────────┐ │
│ │  (empty)                                         │ │
│ └─────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

---

## Control Panel (Forge)

Icon-grid layout, 4 columns, 72×72 px icons:

```
┌─────────────────────────────────────────────────────┐
│  Forge — Control Panel                         [×]  │
├─────────────────────────────────────────────────────┤
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐        │
│  │ DISP   │ │ APPR   │ │ INFO   │ │ CLCK   │        │
│  │Display │ │Appear. │ │SysInfo │ │DateTime│        │
│  └────────┘ └────────┘ └────────┘ └────────┘        │
│  ┌────────┐ ┌────────┐ ┌────────┐                   │
│  │ KBRD   │ │ MOUS   │ │ USER   │                   │
│  │Keyboard│ │ Mouse  │ │ Users  │                   │
│  └────────┘ └────────┘ └────────┘                   │
└─────────────────────────────────────────────────────┘
```

---

## Color Schemes (Themes)

Themes are defined in `desktop/themes/*/theme.conf` (INI format).

| Scheme | Desktop | Window | Titlebar |
|--------|---------|--------|----------|
| Retro Gray | Teal `#008080` | Gray `#c0c0c0` | Navy `#000080` |
| Dark Ember | Black `#1a1a1a` | Dark gray `#2d2d2d` | Ember `#cc4400` |
| Teal Classic | Teal `#008080` | Teal `#009090` | Dark teal `#006060` |
| Mono Classic | Black | White | Black |
| High Contrast | Black | White | Black |

The active theme is loaded at session start by the session manager and applied
to all new windows.
