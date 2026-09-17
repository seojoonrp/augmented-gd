"""Bake the outlined UI fonts (white glyphs, black outline, drop shadow).

Geode's own font pipeline (mod.json resources.fonts) parses an "outline" key
but the CLI (3.9.0) never implements it, so the GD-style UI font is generated
here instead and shipped as plain files (mod.json resources.files). Output
mirrors what Geode produces: <Name>.fnt/.png (sd), <Name>-hd.*, <Name>-uhd.*,
all pages pointing at "<Name>.png" (GD resolves the quality suffix itself).

Run with the Windows Python:  py -3 scripts\\fontgen.py [--force]
Needs Pillow (py -3 -m pip install pillow). build.ps1 runs this after
fontcharset.ps1, so the charset always matches the Korean literals in src/.
"""
import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
TTF = ROOT / "resources" / "fonts" / "ImcreSoojin.ttf"
OUT = ROOT / "resources" / "fonts" / "gen"
MOD_JSON = ROOT / "mod.json"

# size / outline / shadow / tracking are UHD pixels (sd = a quarter), like
# GD's own fonts: 96 ~ goldFont, 64 ~ chatFont. Shadow is straight down.
# Advance = font advance + outline + tracking: the outline ring needs room
# (see build_variant), tracking takes some of it back. ImcreSoojin's side
# bearings are ~3 px each at 96, so -outline/2 still keeps rings off the ink.
FONTS = [
    {"name": "AugName", "size": 96, "outline": 6, "shadow": 4, "tracking": -3},
    {"name": "AugText", "size": 64, "outline": 4, "shadow": 3, "tracking": -2},
]
SHADOW_ALPHA = 110
PAD = 2                      # atlas padding between glyphs
VARIANTS = [("-uhd", 1), ("-hd", 2), ("", 4)]
VERSION = 1                  # bump to force a rebuild after changing the drawing


def read_charset() -> list[int]:
    # fontcharset.ps1 keeps every "charset" in mod.json identical, so any one
    # of them is the list of codepoints the code can display.
    mod = json.loads(MOD_JSON.read_text(encoding="utf-8"))
    fonts = mod.get("resources", {}).get("fonts", {})
    charset = next(iter(fonts.values()))["charset"]
    points = []
    for part in charset.split(","):
        a, _, b = part.partition("-")
        points.extend(range(int(a), (int(b) if b else int(a)) + 1))
    return points


def stamp(points: list[int]) -> str:
    h = hashlib.sha1()
    h.update(TTF.read_bytes())
    h.update(json.dumps(FONTS, sort_keys=True).encode())
    h.update(f"{SHADOW_ALPHA}:{PAD}:{VERSION}".encode())
    h.update(",".join(map(str, points)).encode())
    return h.hexdigest()


def render_glyph(font, ch, outline, shadow):
    """RGBA image of one glyph plus its BMFont offsets, or None if blank."""
    x0, y0, x1, y1 = font.getbbox(ch, stroke_width=outline)
    if x1 <= x0 or y1 <= y0:
        return None
    img = Image.new("RGBA", (x1 - x0, y1 - y0 + shadow), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    black = (0, 0, 0, 255)
    if shadow:
        sh = (0, 0, 0, SHADOW_ALPHA)
        draw.text((-x0, -y0 + shadow), ch, font=font, fill=sh, stroke_width=outline, stroke_fill=sh)
    draw.text((-x0, -y0), ch, font=font, fill=(255, 255, 255, 255), stroke_width=outline, stroke_fill=black)
    if img.getchannel("A").getextrema()[1] == 0:
        return None  # whitespace: the stroke pads the bbox but draws nothing
    return img, x0, y0


def pack(glyphs):
    """Shelf packing. Returns (atlas, {cp: (x, y)})."""
    order = sorted(glyphs, key=lambda cp: -glyphs[cp][0].height)
    area = sum((g[0].width + PAD) * (g[0].height + PAD) for g in glyphs.values())
    widest = max(g[0].width for g in glyphs.values()) + 2 * PAD
    width = max(widest, 1 << math.ceil(math.log2(math.sqrt(area) * 1.05)))
    width = min(width, 4096)

    positions = {}
    x, y, shelf = PAD, PAD, 0
    for cp in order:
        img = glyphs[cp][0]
        if x + img.width + PAD > width:
            x, y, shelf = PAD, y + shelf + PAD, 0
        positions[cp] = (x, y)
        x += img.width + PAD
        shelf = max(shelf, img.height)
    height = y + shelf + PAD

    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for cp, (px, py) in positions.items():
        atlas.paste(glyphs[cp][0], (px, py))
    return atlas, positions


def build_variant(spec, suffix, factor, points, cmap_check):
    px = spec["size"] // factor
    outline = max(1, round(spec["outline"] / factor))
    shadow = round(spec["shadow"] / factor)
    tracking = round(spec.get("tracking", 0) / factor)
    font = ImageFont.truetype(str(TTF), px)
    ascent, descent = font.getmetrics()

    glyphs, chars, skipped = {}, [], []
    for cp in points:
        ch = chr(cp)
        if not cmap_check(cp):
            skipped.append(cp)
            continue
        # The outline ring is baked into the glyph, so neighbours need extra
        # advance or the next glyph's black ring covers this one's white.
        advance = round(font.getlength(ch)) + outline + tracking
        rendered = render_glyph(font, ch, outline, shadow)
        if rendered is None:
            chars.append((cp, 0, 0, 0, 0, 0, 0, advance))
            continue
        img, x0, y0 = rendered
        glyphs[cp] = (img, x0, y0, advance)

    atlas, positions = pack(glyphs)
    for cp, (img, x0, y0, advance) in glyphs.items():
        x, y = positions[cp]
        chars.append((cp, x, y, img.width, img.height, x0, y0, advance))
    chars.sort()

    name = spec["name"]
    atlas.save(OUT / f"{name}{suffix}.png")
    lines = [
        f'info face="{TTF.name}" size={px} bold=0 italic=0 charset="" unicode=1 '
        f"stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1",
        f"common lineHeight={ascent + descent} base={ascent} scaleW={atlas.width} "
        f"scaleH={atlas.height} pages=1 packed=0",
        f'page id=0 file="{name}.png"',
        f"chars count={len(chars)}",
    ]
    lines += [
        f"char id={cp} x={x} y={y} width={w} height={h} xoffset={xo} yoffset={yo} "
        f"xadvance={adv} page=0 chnl=0"
        for cp, x, y, w, h, xo, yo, adv in chars
    ]
    lines += ["kernings count=0", ""]
    (OUT / f"{name}{suffix}.fnt").write_text("\n".join(lines), encoding="utf-8")
    return len(chars), atlas.size, skipped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="rebuild even if inputs are unchanged")
    args = ap.parse_args()

    if not TTF.exists():
        sys.exit(f"fontgen: missing {TTF}")
    points = read_charset()
    OUT.mkdir(parents=True, exist_ok=True)
    stamp_file = OUT / ".stamp"
    current = stamp(points)
    if not args.force and stamp_file.exists() and stamp_file.read_text() == current:
        print("fontgen: up to date")
        return

    try:
        from fontTools.ttLib import TTFont
        cmap = TTFont(str(TTF)).getBestCmap()
        cmap_check = lambda cp: cp in cmap
    except ImportError:
        cmap_check = lambda cp: True  # Pillow draws .notdef boxes for gaps

    for spec in FONTS:
        for suffix, factor in VARIANTS:
            count, size, skipped = build_variant(spec, suffix, factor, points, cmap_check)
            print(f"fontgen: {spec['name']}{suffix or '-sd'}: {count} chars, atlas {size[0]}x{size[1]}")
            if skipped and factor == 1:
                print("fontgen: not in font, skipped: " + " ".join(f"U+{cp:04X}" for cp in skipped))
    stamp_file.write_text(current)


if __name__ == "__main__":
    main()
