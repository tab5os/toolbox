"""Draws icon.png, the launcher icon: an amber toolbox with a ruler's tick
marks along its lid, on a dark slate ground. Shapes only, no fonts, so it
draws the same anywhere.

    .t5sdk/venv/bin/pip install pillow
    .t5sdk/venv/bin/python3 tools/make_icon.py        # writes icon.png

The launcher crops the icon to a circle (Manifest-and-Packaging.md, "App
icon"), so everything that matters sits inside the middle 90%.
"""
import os

from PIL import Image, ImageDraw

SIZE = 256            # the launcher wants 112 px or more; the device resizes it
SS = 4                # drawn 4x larger, then scaled down for smooth edges
S = SIZE * SS

GROUND = (0x1F, 0x2A, 0x33)
BODY = (0xFF, 0xA7, 0x26)       # amber
LID = (0xFF, 0xC4, 0x5C)
SHADE = (0xE0, 0x84, 0x00)
INK = (0x1F, 0x2A, 0x33)
HANDLE = (0xCF, 0xD8, 0xDC)


def px(v):
    return round(v * S)


def rect(d, x0, y0, x1, y1, r, fill):
    d.rounded_rectangle((px(x0), px(y0), px(x1), px(y1)), radius=px(r), fill=fill)


def main():
    img = Image.new("RGB", (S, S), GROUND)
    d = ImageDraw.Draw(img)
    # The handle: a rounded bar over the lid.
    w = px(0.045)
    d.rounded_rectangle((px(0.38), px(0.22), px(0.62), px(0.36)), radius=px(0.05), outline=HANDLE, width=w)
    # Body and lid.
    rect(d, 0.20, 0.42, 0.80, 0.76, 0.06, BODY)
    rect(d, 0.17, 0.32, 0.83, 0.46, 0.05, LID)
    # The seam between lid and body, and a clasp.
    d.rectangle((px(0.20), px(0.455), px(0.80), px(0.48)), fill=SHADE)
    rect(d, 0.455, 0.44, 0.545, 0.54, 0.02, INK)
    # Ruler ticks along the lid: long every fifth.
    for i in range(13):
        x = 0.23 + i * 0.045
        h = 0.07 if i % 4 == 0 else 0.04
        d.rectangle((px(x), px(0.335), px(x + 0.012), px(0.335 + h)), fill=INK)
    # Two dials on the front: a gauge and a plus.
    cx, cy, r = 0.33, 0.63, 0.065
    d.ellipse((px(cx - r), px(cy - r), px(cx + r), px(cy + r)), outline=INK, width=px(0.022))
    d.line((px(cx), px(cy), px(cx + 0.04), px(cy - 0.035)), fill=INK, width=px(0.022))
    cx = 0.67
    d.rectangle((px(cx - 0.06), px(cy - 0.011), px(cx + 0.06), px(cy + 0.011)), fill=INK)
    d.rectangle((px(cx - 0.011), px(cy - 0.06), px(cx + 0.011), px(cy + 0.06)), fill=INK)
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "icon.png")
    img.save(out, optimize=True)
    print(out)


if __name__ == "__main__":
    main()
