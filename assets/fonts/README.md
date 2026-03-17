# Bitmap Fonts for PhoenixOS / AshDE

This directory contains bitmap fonts used by the PhoenixOS framebuffer console
and the AshDE desktop environment.

## Font Format: PSF (PC Screen Font)

PhoenixOS uses **PSF2** (PC Screen Font version 2), the same format used by the
Linux kernel console. PSF2 files contain:

- A 32-byte header identifying the font and its dimensions
- A table of bitmaps, one per glyph, stored as packed rows of bits
- An optional Unicode mapping table

### PSF2 Header

```c
struct psf2_header {
    uint8_t  magic[4];      /* 0x72 0xB5 0x4A 0x86 */
    uint32_t version;       /* 0 */
    uint32_t headersize;    /* Offset to glyph data (usually 32) */
    uint32_t flags;         /* 0 = no unicode table, 1 = has unicode table */
    uint32_t numglyph;      /* Number of glyphs */
    uint32_t bytesperglyph; /* height * ceil(width / 8) */
    uint32_t height;        /* Glyph height in pixels */
    uint32_t width;         /* Glyph width in pixels */
};
```

### Standard Font: system8x16.psf

The system font used by PhoenixOS is an 8×16 pixel bitmap font covering ASCII
(glyphs 0–127) and the extended Latin-1 range (128–255). Each glyph is 16 bytes
(8 pixels wide × 16 pixels tall, 1 bit per pixel).

To generate `system8x16.psf` from the kernel's built-in font data:

```bash
# Extract from Linux kbd package
sudo apt install kbd
ls /usr/share/consolefonts/   # pick a suitable 8x16 font
cp /usr/share/consolefonts/default8x16.psf.gz .
gunzip default8x16.psf.gz
mv default8x16.psf system8x16.psf
```

Or generate from the bitmap arrays in `kernel/drivers/tty/console.c`:

```bash
scripts/gen-font.py kernel/drivers/tty/console.c > assets/fonts/system8x16.psf
```

## Adding Custom Fonts

1. Create a PSF2-format font file
2. Place it in this directory
3. Reference it in your theme's `theme.conf` under `[fonts]`
4. The AshDE font loader reads PSF2 via `gui_load_font(path)` in `lib/libgui/`

## Embedded Fallback

The kernel console (`kernel/drivers/tty/console.c`) contains an inline 8×16
bitmap font for use before any filesystem is mounted. This covers ASCII 32–127
and does not require any external file.
