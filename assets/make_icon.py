#!/usr/bin/env python3
"""Generate the VM Manager app icon set from a single vector-ish design.

Draws a rounded indigo→violet tile with two overlapping "screen" cards (the
virtualization motif) and a running-state dot, then emits:
  - assets/icon_1024.png (master)
  - assets/AppIcon.icns  (macOS, via iconutil)
  - assets/AppIcon.ico   (Windows, multi-size)
  - assets/icon_256.png / icon_512.png (Linux desktop)

Run: python3 assets/make_icon.py
"""
import os, math, subprocess, shutil
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 4  # supersample factor
BASE = 1024

def lerp(a, b, t): return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))

def rounded_mask(size, radius):
    m = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle([0, 0, size - 1, size - 1], radius=radius, fill=255)
    return m

def gradient(size, c0, c1):
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * (size - 1))
            px[x, y] = lerp(c0, c1, t)
    return img

def card(draw, box, radius, fill, outline=None, width=0):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def build(size):
    S = size * SS
    indigo = (0x63, 0x66, 0xF1)
    violet = (0x8B, 0x5C, 0xF6)
    tile = gradient(S, indigo, violet).convert("RGBA")
    tile.putalpha(rounded_mask(S, int(S * 0.225)))

    layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # Back card (offset, translucent white)
    w, h = int(S * 0.46), int(S * 0.34)
    r = int(S * 0.055)
    bx, by = int(S * 0.30), int(S * 0.26)
    card(d, [bx, by, bx + w, by + h], r, (255, 255, 255, 70))

    # Front card (solid white monitor)
    fx, fy = int(S * 0.22), int(S * 0.40)
    card(d, [fx, fy, fx + w, fy + h], r, (255, 255, 255, 255))
    # title bar dots on the front card
    dot_r = int(S * 0.014)
    cy = fy + int(h * 0.16)
    for i, col in enumerate([(0xEF,0x44,0x44), (0xF5,0x9E,0x0B), (0x22,0xC5,0x5E)]):
        cx = fx + int(w * 0.12) + i * int(dot_r * 3.2)
        d.ellipse([cx - dot_r, cy - dot_r, cx + dot_r, cy + dot_r], fill=col + (255,))
    # a "play"/boot triangle centered in the screen
    tri_cx, tri_cy = fx + w // 2, fy + int(h * 0.62)
    ts = int(S * 0.052)
    d.polygon([(tri_cx - ts//2, tri_cy - ts), (tri_cx - ts//2, tri_cy + ts),
               (tri_cx + ts, tri_cy)], fill=(0x63, 0x66, 0xF1, 255))

    out = Image.alpha_composite(tile, layer)
    return out.resize((size, size), Image.LANCZOS)

def main():
    master = build(BASE)
    master.save(os.path.join(HERE, "icon_1024.png"))
    build(512).save(os.path.join(HERE, "icon_512.png"))
    build(256).save(os.path.join(HERE, "icon_256.png"))

    # Windows .ico (multi-size)
    ico_sizes = [(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)]
    master.save(os.path.join(HERE, "AppIcon.ico"), sizes=ico_sizes)

    # macOS .icns via iconset + iconutil
    iconset = os.path.join(HERE, "AppIcon.iconset")
    if os.path.exists(iconset): shutil.rmtree(iconset)
    os.makedirs(iconset)
    specs = [(16,1),(16,2),(32,1),(32,2),(128,1),(128,2),(256,1),(256,2),(512,1),(512,2)]
    for base, scale in specs:
        px = base * scale
        name = f"icon_{base}x{base}{'@2x' if scale==2 else ''}.png"
        build(px).save(os.path.join(iconset, name))
    if shutil.which("iconutil"):
        subprocess.run(["iconutil", "-c", "icns", iconset,
                        "-o", os.path.join(HERE, "AppIcon.icns")], check=True)
        shutil.rmtree(iconset)
    print("Icon set written to", HERE)

if __name__ == "__main__":
    main()
