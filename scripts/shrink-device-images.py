"""Downscale the device hero images in Client/resources/devices.

The app shows them in a ~290 px square (up to ~600 px on HiDPI). The originals
are 1300-1500 px and ~1.5 MB each; bundled through qml.qrc they turn into a
110 MB C++ file that MSVC cannot compile alongside other jobs (C1060). 800 px
keeps every display size crisp at roughly a third of the bytes.

Usage: python scripts/shrink-device-images.py   (idempotent; needs Pillow)
"""
from pathlib import Path

from PIL import Image

MAX_SIDE = 800
ROOT = Path(__file__).resolve().parent.parent
DEVICES = ROOT / "Client" / "resources" / "devices"

total_before = total_after = 0
for path in sorted(DEVICES.glob("*.png")):
    before = path.stat().st_size
    with Image.open(path) as image:
        if max(image.size) <= MAX_SIDE:
            print(f"{path.name:<20} {image.size[0]}x{image.size[1]}  kept")
            total_before += before
            total_after += before
            continue
        scale = MAX_SIDE / max(image.size)
        size = (round(image.size[0] * scale), round(image.size[1] * scale))
        resized = image.convert("RGBA").resize(size, Image.LANCZOS)
    resized.save(path, "PNG", optimize=True)
    after = path.stat().st_size
    total_before += before
    total_after += after
    print(f"{path.name:<20} {image.size[0]}x{image.size[1]} -> {size[0]}x{size[1]}  {before/1024:,.0f} KB -> {after/1024:,.0f} KB")

print(f"\ntotal {total_before/1024/1024:.1f} MB -> {total_after/1024/1024:.1f} MB")
