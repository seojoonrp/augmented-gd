"""Fit an exported mark into resources/ui/aug-logo.png.

CircleButtonSprite scales whatever it is given to 65 % of the circle, so any
transparent margin in the file only makes the mark smaller: this crops the
drawing to its own bounds, pads it 2 % into a square and resamples it to
256 px. 256 is about twice what the buttons draw at uhd (the level page's
Medium circle wants ~125 px, the pause menu's Big ~147), which keeps the
in-game shrink under 2x: GD's textures have no mipmaps, and shrinking more
than that makes thin strokes jagged (a 336 px export looked broken,
2026-09-29).

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
