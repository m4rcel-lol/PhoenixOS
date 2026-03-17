# Wallpapers for PhoenixOS / AshDE

This directory contains wallpaper/desktop background images for AshDE.

## Format

PhoenixOS wallpapers are stored as **24-bit uncompressed BMP** files or raw
**RGB888** pixel data files, both at a standard resolution of **640×480** for
VGA compatibility. Higher resolutions (800×600, 1024×768) may also be provided.

### BMP (recommended)

Standard Windows BMP, 24-bit color depth, no compression, top-down row order.

```
Dimensions: 640×480 (minimum), 800×600, 1024×768
Color depth: 24bpp (RGB888) — no alpha channel
Compression: None (BI_RGB)
```

### Raw RGB

Alternatively, raw pixel files (`.raw`) store pixels as sequential RGB triples,
row-major, top-to-bottom:

```
File size = width × height × 3 bytes
No header
```

## Loading Wallpapers

The AshDE compositor (`desktop/wm/compositor.c`) loads wallpapers via
`load_wallpaper(path)` which:

1. Detects format from magic bytes (`BM` header = BMP, otherwise raw RGB)
2. Scales to current screen resolution using nearest-neighbor if needed
3. Tiles if smaller than the display

## Default Behavior

If no wallpaper file is found, AshDE fills the desktop with the solid teal
color `#008080` (defined as `GUI_DESKTOP` in `lib/libgui/include/gui.h`).

## Creating Wallpapers

Using ImageMagick:

```bash
# Convert any image to 640×480 BMP
convert input.png -resize 640x480! -type TrueColor BMP3:desktop.bmp

# Create a solid teal background
convert -size 640x480 xc:'#008080' BMP3:teal.bmp

# Create a gradient
convert -size 640x480 gradient:'#008080-#004040' BMP3:gradient.bmp
```

## Adding to Themes

Reference wallpapers from a theme configuration:

```ini
[wallpaper]
path   = /assets/wallpapers/default.bmp
mode   = stretch    ; stretch, tile, center, fit
```
