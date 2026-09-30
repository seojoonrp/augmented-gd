"""Draw the cat's placeholder frames into resources/ui/.

    cat-idle-1.png, cat-idle-2.png   the two idle frames the cat swaps between
    cat-cast.png                     the wand swing, shown for a moment per cast
    cat-magic-1.png, cat-magic-2.png the circle cast on each removed hazard, two
                                     shaky frames it flips between
    cat-flash.png                    the flash behind a circle as it appears (a soft
                                     glow and short rays, drawn additively in game)

A dumb-looking doodle cat (user, 2026-09-30: the first, polished version
looked "4K" next to everything else): wobbly hand-drawn lines, flat white
fill, two tiny dot eyes, a crude stick wand. The swing frame is an angry
swat (squinting eyes, open mouth, paw up). Drawn with Pillow at 4x and
downsampled. All frames share the canvas and the cat's position, so
swapping them never makes it jump. Placeholder art until real frames exist;
build.ps1 does not run it.

    py -3 scripts/catgen.py
"""

import math
import random
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "resources" / "ui"

SIZE = 256         # uhd pixels (64 pt at sd); Geode bakes hd / sd from it
SUPER = 4          # supersampling factor
W = SIZE * SUPER   # working canvas (1024)
LW = 24            # line width on the working canvas

INK = (30, 30, 36, 255)
FUR = (255, 255, 255, 255)
PINK = (244, 160, 176, 255)
MOUTH = (226, 92, 108, 255)
STAR = (255, 214, 64, 255)


def waves(seed, amp, count=3):
    rnd = random.Random(seed)
    return [(rnd.randint(2, 5), rnd.uniform(0, 2 * math.pi), amp * rnd.uniform(0.5, 1.0)) for _ in range(count)]


def blob(cx, cy, rx, ry, seed, amp=0.035, rot=0.0, n=160):
    """A lumpy closed ellipse: the radius wanders a few percent."""
    ws = waves(seed, amp)
    c, s = math.cos(rot), math.sin(rot)
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        k = 1 + sum(a * math.sin(f * t + ph) for f, ph, a in ws)
        x, y = rx * k * math.cos(t), ry * k * math.sin(t)
        pts.append((cx + x * c - y * s, cy + x * s + y * c))
    return pts


def wobbly(pts, seed, amp=7.0, steps=14, closed=False):
    """Subdivides a polyline and nudges it sideways, like a shaky hand."""
    rnd = random.Random(seed)
    src = pts + [pts[0]] if closed else pts
    out = []
    for (x0, y0), (x1, y1) in zip(src, src[1:]):
        length = math.hypot(x1 - x0, y1 - y0) or 1
        nx, ny = -(y1 - y0) / length, (x1 - x0) / length
        ph = rnd.uniform(0, 2 * math.pi)
        for i in range(steps):
            t = i / steps
            off = amp * math.sin(t * math.pi * 1.5 + ph) * math.sin(t * math.pi)
            out.append((x0 + (x1 - x0) * t + nx * off, y0 + (y1 - y0) * t + ny * off))
    if not closed:
        out.append(src[-1])
    return out


def ink(d, pts, width=LW, closed=False):
    line = pts + [pts[0]] if closed else pts
    d.line(line, fill=INK, width=width, joint="curve")
    r = width / 2
    for x, y in (line[0], line[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=INK)


def shape(d, pts, fill=FUR, width=LW):
    d.polygon(pts, fill=fill)
    ink(d, pts, width, closed=True)


def curve(p0, p1, p2, steps=40):
    out = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        out.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
                    u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))
    return out


def fat_line(d, pts, radius, fill=FUR):
    """A thick filled stroke with an ink edge (the tail)."""
    for x, y in pts:
        r = radius + LW
        d.ellipse((x - r, y - r, x + r, y + r), fill=INK)
    for x, y in pts:
        d.ellipse((x - radius, y - radius, x + radius, y + radius), fill=fill)


def lopsided_star(cx, cy, r, seed, turn=-100.0):
    rnd = random.Random(seed)
    pts = []
    for i in range(10):
        rr = (r if i % 2 == 0 else r * 0.5) * rnd.uniform(0.85, 1.12)
        a = math.radians(turn + i * 36 + rnd.uniform(-5, 5))
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return pts


def draw_cat(frame):
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cast = frame == "cast"
    idle2 = frame == "idle2"

    # Head pose: idle 2 tilts it a little and lets it sink, as if dozing off.
    hx, hy = (566, 512) if idle2 else (556, 500)
    tilt = math.radians(7 if idle2 else 0)

    def head(p):
        x, y = p[0] - 556, p[1] - 500
        c, s = math.cos(tilt), math.sin(tilt)
        return (hx + x * c - y * s, hy + x * s + y * c)

    def heads(pts):
        return [head(p) for p in pts]

    # --- tail, behind the body
    if idle2:
        tail = curve((790, 880), (960, 860), (900, 660))
    elif cast:
        tail = curve((790, 880), (975, 800), (935, 610))
    else:
        tail = curve((790, 880), (950, 920), (930, 700))
    fat_line(d, wobbly(tail, 11, amp=10, steps=6), 30)

    # --- body: a fat lumpy loaf
    shape(d, blob(600, 770, 255, 205, seed=3))
    # Front paws: two lazy bumps along the bottom.
    for cx in (520, 665):
        paw = wobbly([(cx - 58, 960), (cx - 40, 912), (cx + 40, 912), (cx + 58, 960)], cx, amp=5, steps=8)
        d.polygon(paw, fill=FUR)
        ink(d, paw)

    # --- ears, then the head over their bases
    for ear, inner in (
        ([(410, 440), (428, 262), (530, 365)], [(432, 410), (440, 312), (500, 372)]),
        ([(610, 360), (705, 258), (716, 432)], [(640, 368), (696, 308), (694, 408)]),
    ):
        shape(d, wobbly(heads(ear), sum(ear[1]), amp=6, steps=8, closed=True))
        d.polygon(heads(inner), fill=PINK)
    shape(d, heads(blob(556, 500, 205, 168, seed=5, amp=0.03)))

    # --- face: tiny eyes set too far apart, a small mouth
    if cast:
        # The angry swat: squinting slashes, mouth wide open.
        ink(d, wobbly(heads([(452, 470), (522, 500)]), 1, amp=3, steps=6), 20)
        ink(d, wobbly(heads([(668, 466), (598, 497)]), 2, amp=3, steps=6), 20)
        mouth = wobbly(heads([(528, 548), (590, 544), (580, 608), (538, 610)]), 4, amp=4, steps=6, closed=True)
        shape(d, mouth, fill=MOUTH, width=16)
    else:
        # Blank stare: uneven dots, one a bit lower, and a little ㅅ.
        for (x, y), r in (((476, 508), 15), ((648, 492), 12)):
            cx, cy = head((x, y))
            d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=INK)
        ink(d, heads([(542, 566), (560, 540), (578, 566)]), 11)
    for (x0, y0), (x1, y1) in (((420, 548), (338, 532)), ((424, 578), (344, 590)),
                               ((690, 544), (774, 526)), ((688, 574), (768, 584))):
        ink(d, wobbly(heads([(x0, y0), (x1, y1)]), x0 + y0, amp=4, steps=6), 9)

    # --- the wand: a crooked stick with a lopsided star, and the paw on it
    if cast:
        paw, tip, star_r = (382, 610), (196, 330), 78
        # Crude motion strokes where the stick just was.
        for (x0, y0), (x1, y1), (x2, y2) in (((150, 520), (160, 440), (205, 380)),
                                             ((220, 560), (228, 485), (262, 432)),
                                             ((105, 440), (112, 385), (140, 345))):
            ink(d, wobbly(curve((x0, y0), (x1, y1), (x2, y2), 10), x0, amp=4, steps=3), 12)
    else:
        paw, tip, star_r = (418, 818), (250, 680 + (8 if idle2 else 0)), 64
    ink(d, wobbly([paw, tip], 21, amp=9, steps=10), 22)
    shape(d, lopsided_star(tip[0], tip[1], star_r, 31), fill=STAR, width=18)
    x, y = paw
    shape(d, blob(x, y, 56, 50, seed=8, amp=0.05))
    if cast:
        # A raised paw shows its beans.
        d.ellipse((x - 20, y - 6, x + 20, y + 26), fill=PINK)
        for bx, by in ((-26, -22), (0, -32), (26, -22)):
            d.ellipse((x + bx - 9, y + by - 9, x + bx + 9, y + by + 9), fill=PINK)
        for sx, sy in ((92, 238), (276, 214), (150, 150)):
            ink(d, [(sx - 20, sy), (sx + 20, sy)], 10)
            ink(d, [(sx, sy - 20), (sx, sy + 20)], 10)

    return img.resize((SIZE, SIZE), Image.LANCZOS)


MAGIC_SIZE = 128   # uhd pixels (32 pt at sd)


def draw_magic(frame):
    """A wobbly white ring with a lopsided yellow star, the wand's own. The
    two frames differ only in the wobble and the sparkles, so flipping them
    makes the lines shake like a flipbook doodle."""
    w = MAGIC_SIZE * SUPER
    img = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = w / 2
    # The ring as filled rings of one wobble (a wide PIL line jags at its
    # joins): ink, white, ink, then the hole punched back out.
    ws = waves(100 + frame, 0.05)

    def ring(off, n=200):
        pts = []
        for i in range(n):
            t = 2 * math.pi * i / n
            r = 172 * (1 + sum(a * math.sin(f * t + ph) for f, ph, a in ws)) + off
            pts.append((c + r * math.cos(t), c + r * math.sin(t)))
        return pts

    for off, colour in ((29, INK), (14, FUR), (-14, INK), (-29, (0, 0, 0, 0))):
        d.polygon(ring(off), fill=colour)
    star = lopsided_star(c, c + 6, 100, seed=40 + frame, turn=-100 + 9 * frame)
    shape(d, star, fill=STAR, width=22)
    sparkles = ((88, 104), (432, 404)) if frame == 1 else ((420, 96), (96, 418))
    for sx, sy in sparkles:
        for width, colour in ((34, INK), (14, FUR)):
            d.line([(sx - 30, sy), (sx + 30, sy)], fill=colour, width=width)
            d.line([(sx, sy - 30), (sx, sy + 30)], fill=colour, width=width)
    return img.resize((MAGIC_SIZE, MAGIC_SIZE), Image.LANCZOS)


FLASH_SIZE = 128   # uhd pixels (32 pt at sd)


def draw_flash():
    """A soft round glow, warm in the middle, with eight short rays. White
    on transparent: the game adds it onto the level, so it only brightens."""
    w = FLASH_SIZE * SUPER
    c = w / 2
    img = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    px = img.load()
    radius = w * 0.46
    for y in range(w):
        for x in range(w):
            t = math.hypot(x - c, y - c) / radius
            if t >= 1:
                continue
            a = (1 - t) ** 2
            warm = max(0.0, 1 - t * 2.2)   # a pale yellow core
            px[x, y] = (255, 255, int(255 - 70 * warm), int(235 * a))
    d = ImageDraw.Draw(img)
    for i in range(8):
        ang = math.radians(i * 45 + 22.5)
        inner, outer = (0.50, 0.96) if i % 2 == 0 else (0.54, 0.78)
        cx, cy = math.cos(ang), math.sin(ang)
        nx, ny = -cy, cx
        half = 14
        tip = (c + cx * outer * c, c + cy * outer * c)
        base = (c + cx * inner * c, c + cy * inner * c)
        d.polygon([(base[0] + nx * half, base[1] + ny * half), tip,
                   (base[0] - nx * half, base[1] - ny * half)], fill=(255, 255, 240, 230))
    return img.resize((FLASH_SIZE, FLASH_SIZE), Image.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for frame, name in (("idle1", "cat-idle-1.png"), ("idle2", "cat-idle-2.png"), ("cast", "cat-cast.png")):
        path = OUT / name
        draw_cat(frame).save(path)
        print(f"catgen: {path.relative_to(ROOT)} ({SIZE}x{SIZE})")
    for frame in (1, 2):
        path = OUT / f"cat-magic-{frame}.png"
        draw_magic(frame).save(path)
        print(f"catgen: {path.relative_to(ROOT)} ({MAGIC_SIZE}x{MAGIC_SIZE})")
    path = OUT / "cat-flash.png"
    draw_flash().save(path)
    print(f"catgen: {path.relative_to(ROOT)} ({FLASH_SIZE}x{FLASH_SIZE})")


if __name__ == "__main__":
    main()
