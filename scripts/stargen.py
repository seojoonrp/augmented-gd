"""Bake resources/ui/round-star.png from resources/ui/round-star.svg.

GD cannot load SVG and no SVG renderer is installed here, but the star is a
single path of absolute M plus relative c (cubic) commands, so this flattens
the curves and fills them with Pillow at 4x, then downsamples. The fill is
white (tinted gold / grey in game with setColor) inside a baked black
outline, the same trick as the outlined fonts. One-off: build.ps1 does not
run it; rerun after changing the SVG.

    py -3 scripts/stargen.py
"""

import re
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "resources" / "ui" / "round-star.svg"
OUT = ROOT / "resources" / "ui" / "round-star.png"

SIZE = 96          # uhd pixels (24 pt at sd); Geode bakes hd / sd from it
SUPER = 4          # supersampling factor
OUTLINE = 7        # outline width in final pixels
MARGIN = 1         # clear pixels around the outline
CURVE_STEPS = 40   # points per cubic segment


def path_points(d):
    """Absolute points along an 'M x y c ... z' path (relative cubics only)."""
    tokens = re.findall(r"[MmCcZz]|-?\d*\.?\d+(?:e-?\d+)?", d)
    points = []
    x = y = 0.0
    cmd = None
    i = 0
    while i < len(tokens):
        t = tokens[i]
        if t in "MmCcZz":
            cmd = t
            i += 1
            if cmd in "Zz":
                break
            continue
        if cmd == "M":
            x, y = float(tokens[i]), float(tokens[i + 1])
            points.append((x, y))
            i += 2
            cmd = "c"  # further pairs after M would be lines; not used here
        elif cmd == "c":
            dx1, dy1, dx2, dy2, dx, dy = (float(v) for v in tokens[i:i + 6])
            p0 = (x, y)
            p1 = (x + dx1, y + dy1)
            p2 = (x + dx2, y + dy2)
            p3 = (x + dx, y + dy)
            for s in range(1, CURVE_STEPS + 1):
                u = s / CURVE_STEPS
                a = (1 - u) ** 3
                b = 3 * (1 - u) ** 2 * u
                c = 3 * (1 - u) * u ** 2
                e = u ** 3
                points.append((
                    a * p0[0] + b * p1[0] + c * p2[0] + e * p3[0],
                    a * p0[1] + b * p1[1] + c * p2[1] + e * p3[1],
                ))
            x, y = p3
            i += 6
        else:
            raise ValueError("unsupported path command: " + str(cmd))
    return points


def main():
    svg = SRC.read_text(encoding="utf-8")
    d = re.search(r'\sd="([^"]+)"', svg).group(1)
    pts = path_points(d)

    big = SIZE * SUPER
    stroke = OUTLINE * SUPER
    inner = big - 2 * (stroke + MARGIN * SUPER)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    w = max(xs) - min(xs)
    h = max(ys) - min(ys)
    scale = inner / max(w, h)
    cx = (max(xs) + min(xs)) / 2
    cy = (max(ys) + min(ys)) / 2
    mapped = [((px - cx) * scale + big / 2, (py - cy) * scale + big / 2) for px, py in pts]

    fill = Image.new("L", (big, big), 0)
    ImageDraw.Draw(fill).polygon(mapped, fill=255)

    # Outline = the fill grown by `stroke`: a disc at every boundary point
    # (points are dense) plus the fill itself.
    ring = fill.copy()
    draw = ImageDraw.Draw(ring)
    for px, py in mapped:
        draw.ellipse((px - stroke, py - stroke, px + stroke, py + stroke), fill=255)

    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    img.paste((0, 0, 0, 255), (0, 0), ring)
    img.paste((255, 255, 255, 255), (0, 0), fill)
    img = img.resize((SIZE, SIZE), Image.LANCZOS)
    img.save(OUT)
    print("stargen: {} points -> {} ({}x{}, outline {} px)".format(len(pts), OUT.name, SIZE, SIZE, OUTLINE))


if __name__ == "__main__":
    main()
