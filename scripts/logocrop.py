"""Fit an exported mark into resources/ui/aug-logo.png.

CircleButtonSprite fits the sprite to 65% of the circle, so any transparent
margin just makes the mark smaller. Crops to the drawing, pads 2% into a
square and resizes to 256 px. The buttons draw it at ~125-147 px (uhd) and GD
has no mipmaps, so shrinking more than 2x makes the thin strokes jagged.

    py -3 scripts/logocrop.py path/to/export.png
"""

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "resources" / "ui" / "aug-logo.png"
SIZE = 256
PAD = 0.02


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(1)
    src = Image.open(sys.argv[1]).convert("RGBA")
    bbox = src.split()[3].getbbox()
    if not bbox:
        raise SystemExit("logocrop: the image is fully transparent")
    drawing = src.crop(bbox)
    w, h = drawing.size
    side = round(max(w, h) * (1 + 2 * PAD))
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(drawing, ((side - w) // 2, (side - h) // 2))
    # Resample premultiplied, or the transparent pixels' colour bleeds into
    # the edges as a fringe.
    out = square.convert("RGBa").resize((SIZE, SIZE), Image.LANCZOS).convert("RGBA")
    out.save(OUT)
    print("logocrop: {} {}x{}, drawing {}x{} -> {} {}x{}".format(
        Path(sys.argv[1]).name, src.size[0], src.size[1], w, h, OUT.name, SIZE, SIZE))


if __name__ == "__main__":
    main()
