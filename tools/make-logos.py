#!/usr/bin/env python3
"""Erzeugt die 7 System-Logos launcher/data/logo_<key>.png (64x64, hell, transparent).

Einfache geometrische Formen, keine Markenzeichen. Aufruf aus dem Repo-Root:
    python3 tools/make-logos.py
Braucht Pillow. Gezeichnet wird 4x groß und dann heruntergerechnet (Kantenglättung).
"""
import os
from PIL import Image, ImageDraw, ImageFont

S = 4                      # Supersampling
N = 64 * S
INK = (229, 231, 235, 255)  # #E5E7EB
OUT = os.path.join(os.path.dirname(__file__), "..", "launcher", "data")
FONT = os.path.join(OUT, "font.ttf")


def canvas():
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def s(*v):
    return [x * S for x in v]


def wii(d):
    # Fernbedienung: hohes abgerundetes Rechteck, Steuerkreuz, Knopf
    d.rounded_rectangle(s(24, 6, 40, 58), radius=6 * S, outline=INK, width=3 * S)
    d.rectangle(s(30, 14, 34, 24), fill=INK)
    d.rectangle(s(27, 17, 37, 21), fill=INK)
    d.ellipse(s(29, 30, 35, 36), fill=INK)
    d.ellipse(s(30, 44, 34, 48), fill=INK)
    d.ellipse(s(30, 50, 34, 54), fill=INK)


def gc(d):
    # Würfel in Isometrie
    top = s(32, 8, 54, 20, 32, 32, 10, 20)
    d.polygon(top, outline=INK, width=3 * S)
    d.line(s(10, 20, 10, 44, 32, 56, 54, 44, 54, 20), fill=INK, width=3 * S, joint="curve")
    d.line(s(32, 32, 32, 56), fill=INK, width=3 * S)


def n64(d):
    font = ImageFont.truetype(FONT, 34 * S)
    d.text((32 * S, 33 * S), "64", font=font, fill=INK, anchor="mm")
    d.rounded_rectangle(s(6, 10, 58, 54), radius=8 * S, outline=INK, width=3 * S)


def snes(d):
    # vier Knöpfe als Raute
    for cx, cy in ((32, 14), (50, 32), (32, 50), (14, 32)):
        d.ellipse(s(cx - 8, cy - 8, cx + 8, cy + 8), outline=INK, width=3 * S)


def nes(d):
    # flaches Pad: Rahmen, Steuerkreuz, zwei runde Knöpfe
    d.rounded_rectangle(s(4, 18, 60, 46), radius=3 * S, outline=INK, width=3 * S)
    d.rectangle(s(14, 25, 18, 39), fill=INK)
    d.rectangle(s(9, 30, 23, 34), fill=INK)
    d.ellipse(s(38, 30, 45, 37), fill=INK)
    d.ellipse(s(48, 30, 55, 37), fill=INK)


def sega(d):
    # sechs Knöpfe in zwei versetzten Reihen
    for row, y in enumerate((24, 40)):
        for col in range(3):
            cx = 16 + col * 16 + (0 if row == 0 else 4)
            d.ellipse(s(cx - 6, y - 6 - col * 2, cx + 6, y + 6 - col * 2), fill=INK)


def dos(d):
    # Diskette
    d.polygon(s(8, 8, 48, 8, 56, 16, 56, 56, 8, 56), outline=INK, width=3 * S)
    d.rectangle(s(18, 8, 44, 24), outline=INK, width=3 * S)
    d.rectangle(s(36, 12, 40, 20), fill=INK)
    d.rectangle(s(16, 34, 48, 56), outline=INK, width=3 * S)


LOGOS = {"wii": wii, "gc": gc, "n64": n64, "snes": snes, "nes": nes, "sega": sega, "dos": dos}

if __name__ == "__main__":
    for key, fn in LOGOS.items():
        img, d = canvas()
        fn(d)
        img = img.resize((64, 64), Image.LANCZOS)
        path = os.path.join(OUT, f"logo_{key}.png")
        img.save(path, optimize=True)
        print(path, os.path.getsize(path))
