#!/usr/bin/env bash
#
# Regenerates every packaged icon from the canonical brand source in assets/.
# assets/app-icon.svg is the single source of truth; everything below is a
# derivative and should never be edited by hand.
#
# Requires ImageMagick 7 built with the rsvg delegate (magick -list delegate).
#
#   ./packaging/generate-icons.sh
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/assets/app-icon.svg"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

if [ ! -f "${SRC}" ]; then
    echo "error: brand source not found: ${SRC}" >&2
    exit 1
fi

echo "Brand source: ${SRC}"

# --- Linux -------------------------------------------------------------------
# The scalable icon is the source itself, so hicolor/scalable stays vector.
cp "${SRC}" "${ROOT}/packaging/linux/sony-device-center.svg"
magick -background none "${SRC}" -resize 512x512 -depth 8 -strip \
    "${ROOT}/packaging/linux/sony-device-center.png"
echo "  linux  sony-device-center.svg, sony-device-center.png (512)"

# --- Windows -----------------------------------------------------------------
magick -background none "${SRC}" \
    -define icon:auto-resize=256,128,64,48,32,16 \
    "${ROOT}/packaging/windows/sony-device-center.ico"
echo "  win    sony-device-center.ico (16-256)"

# --- Docs site and README ----------------------------------------------------
# GitHub Pages serves docs/ as the site root, so these have to live there
# rather than being referenced back out into assets/.
cp "${SRC}" "${ROOT}/docs/app-icon.svg"
magick -background none "${SRC}" -resize 256x256 -depth 8 -strip \
    "${ROOT}/docs/app-icon.png"
echo "  docs   app-icon.svg, app-icon.png (256)"

# --- macOS -------------------------------------------------------------------
# .icns is a flat container of typed chunks; the modern types hold PNG data
# verbatim, so it can be assembled without Apple's iconutil.
for px in 16 32 64 128 256 512 1024; do
    magick -background none "${SRC}" -resize "${px}x${px}" -depth 8 -strip \
        "${TMP}/icon_${px}.png"
done

python3 - "${TMP}" "${ROOT}/packaging/macos/AppIcon.icns" <<'PY'
import struct, sys, pathlib

tmp, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])

# (chunk type, pixel size) — Apple's PNG-bearing icon types.
TYPES = [
    (b"icp4", 16), (b"icp5", 32), (b"icp6", 64),
    (b"ic07", 128), (b"ic08", 256), (b"ic09", 512), (b"ic10", 1024),
    (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512),
]

chunks = b"".join(
    kind + struct.pack(">I", len(data) + 8) + data
    for kind, px in TYPES
    for data in [(tmp / f"icon_{px}.png").read_bytes()]
)
out.write_bytes(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)
print(f"  macos  AppIcon.icns ({len(TYPES)} representations)")
PY
