#!/usr/bin/env python3
"""gragen — draws the DMG window background.

    python3 packaging/macos/gragen.py

The picture is the app's own palette: near-black ground, a violet glow behind
the app icon, sound rings rolling toward the Applications folder, and one arrow
that says "drag me". It is 660x400 points, written twice: dmg-background.png at
1x and dmg-background@2x.png at 2x with a 144 DPI tag. dmgbuild folds the pair
into one multi-resolution TIFF, so Finder draws it crisp on any display.

Standard library only. Anyone with a python3 can regenerate it; nobody has to
install anything to change a colour. The committed PNG is the build input, this
file is where it comes from.
"""

import argparse
import math
import random
import struct
import zlib
from pathlib import Path

# Window geometry in points. dmgbuild.py lays the icons out on the same grid.
WIDTH, HEIGHT = 660, 400
APP_ICON = (175, 185)
APPLICATIONS_ICON = (485, 185)

# Colours from apps/device-center/qml/Main.qml. No orphan hex.
BG = (0x0A, 0x0B, 0x0F)
BG_WARM = (0x18, 0x14, 0x2A)
ACCENT = (0x7C, 0x5C, 0xFF)
ACCENT_SOFT = (0xA7, 0x8B, 0xFA)
TXT_DIM = (0x98, 0xA1, 0xB2)


def lerp(a, b, t):
    return a + (b - a) * t


def mix(c1, c2, t):
    return tuple(lerp(a, b, t) for a, b in zip(c1, c2))


def over(base, colour, alpha):
    """Composite `colour` over `base` with the given coverage."""
    return mix(base, colour, max(0.0, min(1.0, alpha)))


def smoothstep(edge0, edge1, x):
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3 - 2 * t)


def coverage(distance, feather=0.75):
    """Anti-aliased coverage for a signed distance (negative = inside)."""
    return 1.0 - smoothstep(-feather, feather, distance)


def segment_distance(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    t = ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby)
    t = max(0.0, min(1.0, t))
    cx, cy = ax + abx * t, ay + aby * t
    return math.hypot(px - cx, py - cy)


class Arrow:
    """A shaft with a chevron head, from the app icon toward Applications."""

    def __init__(self, y, x0, x1, weight=5.0, head=22.0):
        self.y, self.x0, self.x1 = y, x0, x1
        self.weight = weight
        self.head = head
        self.tip = (x1, y)
        self.wings = [(x1 - head, y - head * 0.78), (x1 - head, y + head * 0.78)]

    def distance(self, x, y):
        shaft = segment_distance(x, y, self.x0, self.y, self.x1 - self.weight, self.y)
        wing_a = segment_distance(x, y, *self.tip, *self.wings[0])
        wing_b = segment_distance(x, y, *self.tip, *self.wings[1])
        return min(shaft, wing_a, wing_b) - self.weight / 2


def render(scale):
    w, h = WIDTH * scale, HEIGHT * scale
    ax, ay = APP_ICON
    arrow = Arrow(y=ay, x0=ax + 105, x1=APPLICATIONS_ICON[0] - 105)
    grain = random.Random(1000)  # deterministic: the same PNG on every machine

    rows = []
    for j in range(h):
        y = j / scale
        row = bytearray()
        for i in range(w):
            x = i / scale

            # Ground: a diagonal fade from cold black to a warmer violet-black.
            diag = (x / WIDTH + y / HEIGHT) / 2
            c = mix(BG, BG_WARM, diag)

            # Glow behind the app icon, so the thing to drag reads as lit.
            d = math.hypot(x - ax, y - ay)
            c = over(c, ACCENT, 0.42 * math.exp(-(d / 150) ** 2))

            # Sound rings rolling from the icon toward Applications. They fade
            # with distance and are strongest on the side the arrow points to.
            if d > 95:
                ring = abs((d - 95) % 46 - 23)
                facing = 0.35 + 0.65 * smoothstep(-0.4, 0.9, (x - ax) / max(d, 1))
                strength = 0.16 * facing * math.exp(-(d - 95) / 380)
                c = over(c, ACCENT_SOFT, coverage(ring - 0.8) * strength)

            # The arrow.
            c = over(c, TXT_DIM, coverage(arrow.distance(x, y), 0.6 / scale) * 0.85)

            # A whisper of grain kills gradient banding on wide monitors.
            g = grain.uniform(-1.5, 1.5)
            row.extend(int(round(max(0, min(255, ch + g)))) for ch in c)
        rows.append(bytes(row))
    return w, h, rows


def png_chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def write_png(path, w, h, rows, dpi):
    raw = b"".join(b"\x00" + row for row in rows)  # filter type 0 on every scanline
    px_per_metre = int(round(dpi / 0.0254))
    png = b"\x89PNG\r\n\x1a\n"
    png += png_chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += png_chunk(b"pHYs", struct.pack(">IIB", px_per_metre, px_per_metre, 1))
    png += png_chunk(b"IDAT", zlib.compress(raw, 9))
    png += png_chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", type=Path, default=Path(__file__).with_name("dmg-background.png"),
                        help="1x output path; the @2x file is written beside it")
    args = parser.parse_args()

    for scale in (1, 2):
        out = args.out if scale == 1 else args.out.with_name(f"{args.out.stem}@2x{args.out.suffix}")
        w, h, rows = render(scale)
        write_png(out, w, h, rows, dpi=72 * scale)
        print(f"{out} {w}x{h}px @{scale}x ({WIDTH}x{HEIGHT}pt)")


if __name__ == "__main__":
    main()
