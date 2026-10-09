#!/usr/bin/env python3
"""
BEEP host-side face preview (no third-party dependencies).

This script mirrors the geometry + expression tables used by the Arduino
firmware so the face can be inspected BEFORE the OLED hardware exists.
It renders the same 128x64 procedural face to:
  * ASCII art in the terminal, and/or
  * 1-bit-style grayscale PNG files (written with the stdlib zlib module).

It is intentionally a single file with no build system.  It is a *specification
preview*, not the firmware; the firmware in the parent folder is the real target.

Usage:
  python beep_preview.py --list
  python beep_preview.py --ascii idle
  python beep_preview.py --all-ascii
  python beep_preview.py --png            # writes preview/out/*.png
"""
from __future__ import annotations

import argparse
import math
import os
import struct
import zlib

# --------------------------------------------------------------------------
# Geometry (mirrors config.h defaults)
# --------------------------------------------------------------------------
SCREEN_W = 128
SCREEN_H = 64

EYE_HALF_W = 16.0        # half width of one eye
EYE_CX_OFFSET = 24.0     # horizontal distance of each eye from screen centre
EYE_CY = 30.0            # vertical centre of the eyes
DEFAULT_SHAPE = 2.6      # superellipse exponent (2 = perfect ellipse)

BROW_OFFSET_Y = 20.0     # brow baseline above eye centre
BROW_HALF_W = 9.0        # half length of a brow

MOUTH_BASE_Y = 53.0      # nominal mouth centre (screen space)

# --------------------------------------------------------------------------
# Expression parameter table (mirrors expressions.cpp)
# Positive lidSlant / browTilt => inner corner moves DOWN (angry).
# --------------------------------------------------------------------------
# keys:
# top, bot      : upper/lower eye boundary bulge in px (signed)
# shape         : superellipse exponent
# pupil         : pupil radius as fraction of half width
# pdx, pdy      : pupil gaze bias in px (+right, +down)
# slant         : upper-lid corner slant
# squint        : extra lower-lid raise 0..1
# browY, browTilt, browShow : brow placement
# hl            : highlight size 0..1
# pupilShape    : "round" | "heart"
# mouth, mouthW, mouthOpen, mouthDepth : mouth description
# asym          : per-eye asymmetry amount -1..1
DEFAULTS = dict(
    top=17.0, bot=15.0, shape=DEFAULT_SHAPE,
    pupil=0.42, pdx=0.0, pdy=0.0, slant=0.0, squint=0.0,
    browY=0.0, browTilt=0.0, browShow=0.0,
    hl=0.7, pupilShape="round",
    mouth="none", mouthW=13.0, mouthOpen=0.0, mouthDepth=4.0,
    asym=0.0,
)

EXPRESSIONS = {
    "idle": {},
    "happy": dict(top=15.0, bot=11.0, squint=0.15, pupil=0.44, hl=0.8,
                  mouth="smile", mouthW=13.0, mouthDepth=5.0),
    "excited": dict(top=19.0, bot=17.0, pupil=0.50, pdy=-1.0, hl=1.0,
                    mouth="bigsmile", mouthW=16.0, mouthOpen=1.0, mouthDepth=7.0),
    "curious": dict(top=17.0, bot=15.0, pdx=4.0, pdy=-2.0, asym=0.6,
                    browShow=0.5, browTilt=-0.4, hl=0.8,
                    mouth="o", mouthW=3.5, mouthDepth=3.0),
    "surprised": dict(top=21.0, bot=19.0, pupil=0.28, hl=0.9,
                      browShow=1.0, browY=-3.0, browTilt=-0.2,
                      mouth="o", mouthW=5.0, mouthDepth=5.0),
    "sleepy": dict(top=9.0, bot=8.0, pupil=0.40, pdy=2.0, squint=0.1,
                   slant=1.5, hl=0.5, mouth="flat", mouthW=8.0),
    "sleeping": dict(top=1.4, bot=2.6, pupil=0.0, hl=0.0,
                     mouth="none"),
    "sad": dict(top=12.0, bot=12.0, pupil=0.40, pdy=2.0,
                slant=-3.0, browShow=1.0, browY=1.0, browTilt=-1.0,
                hl=0.6, mouth="frown", mouthW=9.0, mouthDepth=3.0),
    "annoyed": dict(top=9.0, bot=13.0, pupil=0.34, pdx=-1.0, pdy=1.0,
                    slant=5.0, browShow=1.0, browY=2.0, browTilt=1.0,
                    hl=0.5, mouth="frown", mouthW=8.0, mouthDepth=2.0),
    "affectionate": dict(top=14.0, bot=13.0, squint=0.2, pupil=0.58,
                         pupilShape="heart", hl=0.9,
                         mouth="smile", mouthW=11.0, mouthDepth=4.0),
    "thinking": dict(top=13.0, bot=13.0, pupil=0.42, pdx=5.0, pdy=-4.0,
                     asym=0.4, browShow=0.7, browTilt=-0.6, browY=-1.0,
                     hl=0.7, mouth="flat", mouthW=5.0),
    "listening": dict(top=18.0, bot=16.0, pupil=0.46, pdy=-0.5,
                      browShow=0.6, browY=-1.5, browTilt=-0.2, hl=0.8,
                      mouth="o", mouthW=3.0, mouthDepth=2.5),
    "confused": dict(top=15.0, bot=15.0, pupil=0.40, pdx=3.0, pdy=-2.0,
                     asym=0.8, browShow=1.0, browY=0.0, browTilt=-0.5,
                     hl=0.7, mouth="wavy", mouthW=12.0, mouthDepth=2.0),
}


def params(name: str) -> dict:
    p = dict(DEFAULTS)
    p.update(EXPRESSIONS[name])
    return p


# --------------------------------------------------------------------------
# Framebuffer
# --------------------------------------------------------------------------
class Frame:
    def __init__(self, w=SCREEN_W, h=SCREEN_H):
        self.w = w
        self.h = h
        self.px = bytearray(w * h)  # 0 = black, 1 = white

    def set(self, x, y, on=True):
        xi, yi = int(round(x)), int(round(y))
        if 0 <= xi < self.w and 0 <= yi < self.h:
            self.px[yi * self.w + xi] = 1 if on else 0

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.px[y * self.w + x]
        return 0


# --------------------------------------------------------------------------
# Procedural eye rendering (mirrors renderer.cpp)
# --------------------------------------------------------------------------
def _profile(t, shape):
    """Superellipse vertical profile in [0,1] for t in [-1,1]."""
    at = abs(t)
    if at >= 1.0:
        return 0.0
    return max(0.0, (1.0 - at ** shape) ** (1.0 / shape))


def draw_eye(f: Frame, cx, cy, half_w, top, bot, shape, slant, squint,
             pupil_visible, pupil_shape, pupil_r, pdx, pdy, hl,
             inner_sign=1.0):
    """Render one eye.  inner_sign = +1 if the face centre is to the +x side."""
    x0 = int(math.floor(cx - half_w))
    x1 = int(math.ceil(cx + half_w))
    topA = {}
    botA = {}
    for ix in range(x0, x1 + 1):
        t = (ix - cx) / half_w
        if abs(t) > 1.0:
            continue
        prof = _profile(t, shape)
        if prof <= 0.0:
            continue
        # inner corner is at t = inner_sign
        topY = cy - top * prof + slant * (t * inner_sign)
        botY = cy + bot * prof
        # squint raises the lower lid toward the centre of the eye
        if squint > 0.0:
            botY -= squint * bot * 0.6 * prof
        if botY < topY:
            mid = (topY + botY) * 0.5
            topY = botY = mid
        topA[ix] = topY
        botA[ix] = botY
        for iy in range(int(math.ceil(topY)), int(math.floor(botY)) + 1):
            f.set(ix, iy, True)

    if not pupil_visible or pupil_r <= 0.5:
        return

    pcx = cx + pdx
    pcy = cy + pdy
    r = pupil_r
    # iterate the pupil bounding circle, clip to the aperture
    for ix in range(int(math.floor(pcx - r)), int(math.ceil(pcx + r)) + 1):
        if ix not in topA:
            continue
        for iy in range(int(math.floor(pcy - r)), int(math.ceil(pcy + r)) + 1):
            if not (topA[ix] <= iy <= botA[ix]):
                continue
            dx = (ix - pcx) / r
            dy = (iy - pcy) / r
            if pupil_shape == "heart":
                u, v = dx, -dy  # v up
                u *= 1.05
                v *= 1.05
                val = (u * u + v * v - 1.0) ** 3 - u * u * v ** 3
                inside = val <= 0.0
            else:
                inside = (dx * dx + dy * dy) <= 1.0
            if inside:
                f.set(ix, iy, False)

    if hl > 0.0:
        hr = max(0.8, r * 0.30 * hl)
        hx = pcx - r * 0.38
        hy = pcy - r * 0.42
        for ix in range(int(math.floor(hx - hr)), int(math.ceil(hx + hr)) + 1):
            for iy in range(int(math.floor(hy - hr)), int(math.ceil(hy + hr)) + 1):
                if (ix - hx) ** 2 + (iy - hy) ** 2 <= hr * hr:
                    # only inside the pupil area
                    dx = (ix - pcx) / r if r > 0 else 9
                    dy = (iy - pcy) / r if r > 0 else 9
                    if pupil_shape == "heart":
                        u, v = dx * 1.05, -dy * 1.05
                        inside = (u * u + v * v - 1.0) ** 3 - u * u * v ** 3 <= 0.0
                    else:
                        inside = (dx * dx + dy * dy) <= 1.0
                    if inside:
                        f.set(ix, iy, True)


# --------------------------------------------------------------------------
# Mouth
# --------------------------------------------------------------------------
def _hline(f, x0, x1, y, thick=2):
    for x in range(int(x0), int(x1) + 1):
        for k in range(thick):
            f.set(x, y + k, True)


def _thick_point(f, x, y):
    f.set(x, y, True)
    f.set(x + 1, y, True)
    f.set(x, y + 1, True)
    f.set(x + 1, y + 1, True)


def draw_mouth(f, kind, cx, cy, half_w, open_amt, depth):
    if kind == "none":
        return
    if kind == "flat":
        _hline(f, cx - half_w, cx + half_w, cy, 2)
    elif kind == "smile":
        for x in range(int(cx - half_w), int(cx + half_w) + 1):
            t = (x - cx) / half_w
            y = cy - depth * (t * t)
            _thick_point(f, x, round(y))
    elif kind == "frown":
        for x in range(int(cx - half_w), int(cx + half_w) + 1):
            t = (x - cx) / half_w
            y = cy + depth * (t * t)
            _thick_point(f, x, round(y))
    elif kind == "bigsmile":
        # arc plus a filled lower region -> open happy mouth
        for x in range(int(cx - half_w), int(cx + half_w) + 1):
            t = (x - cx) / half_w
            top_y = cy - depth * (t * t)
            bot_y = cy + max(1.0, depth * 0.7 * (1 - t * t)) * open_amt
            for y in range(int(round(top_y)), int(round(bot_y)) + 1):
                f.set(x, y, True)
    elif kind == "o":
        r = half_w
        for ix in range(int(cx - r), int(cx + r) + 1):
            for iy in range(int(cy - r), int(cy + r) + 1):
                d = math.sqrt((ix - cx) ** 2 + (iy - cy) ** 2)
                if d <= r and d >= r - 2.0:
                    f.set(ix, iy, True)
    elif kind == "wavy":
        for x in range(int(cx - half_w), int(cx + half_w) + 1):
            t = (x - cx) / half_w
            y = cy + math.sin(t * math.pi * 2.0) * depth
            _thick_point(f, x, round(y))


# --------------------------------------------------------------------------
# Brow
# --------------------------------------------------------------------------
def draw_brow(f, cx, cy, inner_sign, y_off, tilt, show):
    if show <= 0.05:
        return
    inner_x = cx + inner_sign * BROW_HALF_W
    outer_x = cx - inner_sign * BROW_HALF_W
    inner_y = cy + y_off + tilt * 3.0
    outer_y = cy + y_off - tilt * 3.0
    steps = int(abs(inner_x - outer_x))
    for i in range(steps + 1):
        t = i / max(1, steps)
        x = outer_x + (inner_x - outer_x) * t
        y = outer_y + (inner_y - outer_y) * t
        _thick_point(f, round(x), round(y))


# --------------------------------------------------------------------------
# Face composition (mirrors renderer.cpp drawFace)
# --------------------------------------------------------------------------
def draw_face(p: dict, frame: Frame | None = None) -> Frame:
    f = frame if frame is not None else Frame()
    cxL = SCREEN_W / 2 - EYE_CX_OFFSET
    cxR = SCREEN_W / 2 + EYE_CX_OFFSET

    # per-eye asymmetry: +asym raises/opens the right eye
    asym = p["asym"]
    tL, bL = p["top"] * (1 - 0.18 * asym), p["bot"]
    tR, bR = p["top"] * (1 + 0.18 * asym), p["bot"]
    cyL = EYE_CY + 2.0 * asym
    cyR = EYE_CY - 2.0 * asym

    # upper-lid slant: positive = inner corner down.  left inner is +x.
    slantL = p["slant"] + (p.get("slantAsym", 0.0) * asym)
    slantR = p["slant"] - (p.get("slantAsym", 0.0) * asym)

    pr = p["pupil"] * EYE_HALF_W
    draw_eye(f, cxL, cyL, EYE_HALF_W, tL, bL, p["shape"], slantL, p["squint"],
             p["pupil"] > 0.0, p["pupilShape"], pr, p["pdx"], p["pdy"],
             p["hl"], inner_sign=1.0)
    draw_eye(f, cxR, cyR, EYE_HALF_W, tR, bR, p["shape"], slantR, p["squint"],
             p["pupil"] > 0.0, p["pupilShape"], pr, p["pdx"], p["pdy"],
             p["hl"], inner_sign=-1.0)

    draw_brow(f, cxL, cyL - BROW_OFFSET_Y, 1.0, p["browY"], p["browTilt"],
              p["browShow"])
    draw_brow(f, cxR, cyR - BROW_OFFSET_Y, -1.0, p["browY"], p["browTilt"],
              p["browShow"])

    draw_mouth(f, p["mouth"], SCREEN_W / 2, MOUTH_BASE_Y, p["mouthW"],
               p["mouthOpen"], p["mouthDepth"])
    return f


# --------------------------------------------------------------------------
# Output helpers
# --------------------------------------------------------------------------
def to_ascii(f: Frame) -> str:
    lines = []
    for y in range(f.h):
        line = "".join("#" if f.get(x, y) else "." for x in range(f.w))
        lines.append(line)
    return "\n".join(lines)


def write_png(f: Frame, path: str):
    """Write an 8-bit grayscale PNG (white face on black)."""
    raw = bytearray()
    for y in range(f.h):
        raw.append(0)  # filter type 0
        for x in range(f.w):
            raw.append(255 if f.get(x, y) else 0)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", f.w, f.h, 8, 0, 0, 0, 0)
    idat = zlib.compress(bytes(raw), 9)
    with open(path, "wb") as fh:
        fh.write(sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) +
                 chunk(b"IEND", b""))


def main():
    ap = argparse.ArgumentParser(description="BEEP face preview")
    ap.add_argument("--list", action="store_true", help="list expressions")
    ap.add_argument("--ascii", metavar="EXPR", help="print one expression")
    ap.add_argument("--all-ascii", action="store_true", help="print all")
    ap.add_argument("--png", action="store_true", help="export all to PNG")
    args = ap.parse_args()

    if args.list:
        print(" ".join(EXPRESSIONS.keys()))
        return
    if args.ascii:
        print(to_ascii(draw_face(params(args.ascii))))
        return
    if args.all_ascii:
        for name in EXPRESSIONS:
            print("=== %s ===" % name)
            print(to_ascii(draw_face(params(name))))
            print()
        return
    if args.png:
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
        os.makedirs(out, exist_ok=True)
        for name in EXPRESSIONS:
            write_png(draw_face(params(name)), os.path.join(out, name + ".png"))
        print("wrote %d PNGs to %s" % (len(EXPRESSIONS), out))
        return
    ap.print_help()


if __name__ == "__main__":
    main()
