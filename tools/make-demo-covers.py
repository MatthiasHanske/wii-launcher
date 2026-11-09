#!/usr/bin/env python3
"""Erzeugt den Demo-Spielebaum und eigene Demo-Cover für Dolphin (tools/dolphin-sd/).

Alle Titel sind frei erfunden. Die Cover sind schlichte, eigenständige Grafiken
(Farbverlauf, geometrische Flächen, Titel in Inter aus launcher/data/font.ttf), ohne
Logos, Verpackungsmotive oder Altersfreigaben. So darf das Repo die Bilder enthalten
und die Screenshots für das README zeigen keine fremde Boxart.

Aufruf aus dem Repo-Root (Pillow ist nicht vorausgesetzt, uv holt es temporär):
    uv run --with pillow python3 tools/make-demo-covers.py

Schreibt:
  tools/dolphin-sd/launcher/mock/...         Spiele-Attrappen (wenige Bytes Text), nur wenn sie fehlen
  tools/dolphin-sd/config/images/2D/<ID>.png Wii/GameCube (wie USB Loader GX)
  tools/dolphin-sd/launcher/covers/<system>/<Name>.png  Emulatoren und Adventure (core/covers.cpp)
Alle Cover 160x224 (Seitenverhältnis der 2D-Cover). Gezeichnet wird 4x groß und dann
heruntergerechnet (Kantenglättung). Die Ausgabe ist deterministisch.
"""
import hashlib
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
SD = os.path.join(ROOT, "tools", "dolphin-sd")
MOCK = os.path.join(SD, "launcher", "mock")
FONT = os.path.join(ROOT, "launcher", "data", "font.ttf")

W, H = 160, 224
S = 4  # Supersampling

MOCK_TEXT = b"Demo-Datei des Wii-Launchers, kein Spiel.\n"

# (System, Pfad der Attrappe relativ zu mock/, Coverpfad relativ zu tools/dolphin-sd/,
#  angezeigter Titel, Farbverlauf oben, unten, Motiv)
GAMES = [
    ("wii", "wbfs/Wolkenrennen (Europe) [ZWRP01]/ZWRP01.wbfs", "config/images/2D/ZWRP01.png",
     "Wolkenrennen", "#38BDF8", "#1E3A8A", "clouds"),
    ("wii", "wbfs/Leuchtturm [ZLTP01]/ZLTP01.wbfs", "config/images/2D/ZLTP01.png",
     "Leuchtturm", "#F59E0B", "#7C2D12", "beam"),
    ("wii", "wbfs/Gartenliga [ZGLP01]/ZGLP01.wbfs", "config/images/2D/ZGLP01.png",
     "Gartenliga", "#84CC16", "#14532D", "rings"),
    ("wii", "wbfs/Orbit Kicker [ZOKP01]/ZOKP01.wbfs", "config/images/2D/ZOKP01.png",
     "Orbit Kicker", "#A78BFA", "#1E1B4B", "orbit"),
    ("wii", "wbfs/Kristalltanz (Europe) [ZKTP01]/ZKTP01.wbfs", "config/images/2D/ZKTP01.png",
     "Kristalltanz", "#F472B6", "#4C0519", "shards"),
    ("wii", "wbfs/Funkenflug [ZFFP01]/ZFFP01.wbfs", "config/images/2D/ZFFP01.png",
     "Funkenflug", "#FB923C", "#450A0A", "sparks"),
    ("gc", "games/Nebelbucht [ZNBP01]/game.iso", "config/images/2D/ZNBP01.png",
     "Nebelbucht", "#5EEAD4", "#134E4A", "waves"),
    ("gc", "games/Eisfuchs [ZEFP01]/game.iso", "config/images/2D/ZEFP01.png",
     "Eisfuchs", "#E0F2FE", "#0C4A6E", "peaks"),
    ("n64", "not64/roms/Pixelpiloten (E).z64", "launcher/covers/n64/Pixelpiloten (E).png",
     "Pixelpiloten", "#FDE047", "#9A3412", "grid"),
    ("snes", "snes9xgx/roms/Drachenpfad (Europe).sfc", "launcher/covers/snes/Drachenpfad (Europe).png",
     "Drachenpfad", "#4ADE80", "#1E293B", "path"),
    ("nes", "fceugx/roms/Blockgarten.nes", "launcher/covers/nes/Blockgarten.png",
     "Blockgarten", "#F87171", "#1F2937", "blocks"),
    ("sega", "genplus/roms/md/Neon Brandung.md", "launcher/covers/sega/Neon Brandung.png",
     "Neon Brandung", "#22D3EE", "#312E81", "stripes"),
    ("dos", "scummvm/Das Uhrwerk-Archiv/ARCHIV.000", "launcher/covers/dos/Das Uhrwerk-Archiv.png",
     "Das Uhrwerk-Archiv", "#D6B88A", "#3B2410", "gears"),
    ("dos", "dosbox/Kellerlabor/START.BAT", "launcher/covers/dos/Kellerlabor.png",
     "Kellerlabor", "#94A3B8", "#0F172A", "terminal"),
]


def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def gradient(top, bottom):
    """Senkrechter Verlauf mit leichter Diagonale."""
    img = Image.new("RGB", (W * S, H * S))
    px = img.load()
    for y in range(H * S):
        for x in range(0, W * S):
            t = min(1.0, max(0.0, (y / (H * S)) * 0.85 + (x / (W * S)) * 0.15))
            px[x, y] = mix(top, bottom, t)
    return img


def overlay(base, draw_fn, alpha):
    """Formen auf eigener Ebene zeichnen und mit Deckkraft alpha über base legen."""
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    draw_fn(d)
    if alpha < 1:
        a = layer.getchannel("A").point(lambda v: round(v * alpha))
        layer.putalpha(a)
    base.alpha_composite(layer)


def s(*v):
    return [x * S for x in v]


# Motive: abstrakte Flächen, nur Kreise, Polygone, Linien.
def motif(name, img, light, dark, rnd):
    white = (255, 255, 255, 255)
    if name == "clouds":
        def f(d):
            for cx, cy, r in ((40, 70, 28), (70, 60, 34), (104, 72, 26), (120, 112, 22), (30, 118, 18)):
                d.ellipse(s(cx - r, cy - r, cx + r, cy + r), fill=white)
        overlay(img, f, 0.28)
        overlay(img, lambda d: d.line(s(10, 150, 150, 40), fill=white, width=3 * S), 0.6)
    elif name == "beam":
        def f(d):
            d.polygon(s(80, 50, 160, 10, 160, 90), fill=white)
            d.polygon(s(80, 50, 0, 20, 0, 70), fill=white)
        overlay(img, f, 0.22)
        overlay(img, lambda d: d.polygon(s(72, 140, 88, 140, 84, 56, 76, 56), fill=white), 0.85)
        overlay(img, lambda d: d.ellipse(s(72, 42, 88, 58), fill=white), 0.95)
    elif name == "rings":
        def f(d):
            for i, r in enumerate((70, 54, 38, 22)):
                d.ellipse(s(80 - r, 80 - r, 80 + r, 80 + r), outline=white, width=(3 if i % 2 else 6) * S)
        overlay(img, f, 0.35)
        overlay(img, lambda d: d.ellipse(s(72, 72, 88, 88), fill=white), 0.9)
    elif name == "orbit":
        def f(d):
            d.ellipse(s(14, 50, 146, 110), outline=white, width=2 * S)
            d.ellipse(s(40, 26, 120, 134), outline=white, width=2 * S)
        overlay(img, f, 0.45)
        overlay(img, lambda d: d.ellipse(s(62, 62, 98, 98), fill=light + (255,)), 1.0)
        overlay(img, lambda d: d.ellipse(s(132, 66, 144, 78), fill=white), 0.95)
    elif name == "shards":
        def f(d):
            for _ in range(9):
                cx, cy = rnd.uniform(10, 150), rnd.uniform(10, 140)
                r = rnd.uniform(14, 34)
                a0 = rnd.uniform(0, math.pi)
                pts = []
                for k in range(3):
                    a = a0 + k * 2 * math.pi / 3 + rnd.uniform(-0.3, 0.3)
                    pts += [(cx + math.cos(a) * r) * S, (cy + math.sin(a) * r) * S]
                d.polygon(pts, fill=(255, 255, 255, rnd.randint(60, 160)))
        overlay(img, f, 0.6)
    elif name == "sparks":
        def f(d):
            for _ in range(40):
                x, y = rnd.uniform(0, 160), rnd.uniform(0, 150)
                r = rnd.uniform(1, 4)
                d.ellipse(s(x - r, y - r, x + r, y + r), fill=(255, 240, 200, rnd.randint(120, 255)))
        overlay(img, f, 0.9)
        overlay(img, lambda d: d.arc(s(-40, 20, 200, 260), 200, 340, fill=white, width=3 * S), 0.5)
    elif name == "waves":
        def f(d):
            for i in range(6):
                y0 = 40 + i * 18
                pts = [(x * S, (y0 + math.sin(x / 18 + i) * 6) * S) for x in range(0, 161, 4)]
                d.line(pts, fill=white, width=2 * S, joint="curve")
        overlay(img, f, 0.45)
    elif name == "peaks":
        def f(d):
            d.polygon(s(-10, 150, 50, 50, 110, 150), fill=white)
            d.polygon(s(50, 150, 110, 70, 170, 150), fill=white)
        overlay(img, f, 0.35)
        overlay(img, lambda d: d.polygon(s(50, 50, 62, 70, 38, 70), fill=white), 0.9)
    elif name == "grid":
        def f(d):
            for i in range(0, 161, 16):
                d.line(s(i, 0, i, 150), fill=white, width=1 * S)
            for j in range(0, 151, 16):
                d.line(s(0, j, 160, j), fill=white, width=1 * S)
        overlay(img, f, 0.18)
        def g(d):
            for _ in range(10):
                x, y = rnd.randrange(0, 10) * 16, rnd.randrange(0, 9) * 16
                d.rectangle(s(x, y, x + 16, y + 16), fill=white)
        overlay(img, g, 0.35)
    elif name == "path":
        def f(d):
            pts = [((80 + math.sin(y / 22) * 40) * S, y * S) for y in range(0, 151, 3)]
            d.line(pts, fill=white, width=10 * S, joint="curve")
        overlay(img, f, 0.3)
        overlay(img, lambda d: d.ellipse(s(110, 20, 140, 50), outline=(255, 255, 255, 255), width=3 * S), 0.7)
    elif name == "blocks":
        def f(d):
            for _ in range(14):
                x, y = rnd.randrange(0, 8) * 20, rnd.randrange(0, 7) * 20
                d.rectangle(s(x + 2, y + 2, x + 18, y + 18), fill=(255, 255, 255, rnd.randint(80, 255)))
        overlay(img, f, 0.5)
    elif name == "stripes":
        def f(d):
            for i in range(-6, 12):
                x = i * 22
                d.polygon(s(x, 150, x + 10, 150, x + 90, 0, x + 80, 0), fill=white)
        overlay(img, f, 0.2)
        overlay(img, lambda d: d.ellipse(s(96, 18, 140, 62), fill=light + (255,)), 0.9)
    elif name == "gears":
        def gear(d, cx, cy, r, teeth):
            pts = []
            for k in range(teeth * 2):
                a = k * math.pi / teeth
                rr = r if k % 2 == 0 else r * 0.8
                pts += [(cx + math.cos(a) * rr) * S, (cy + math.sin(a) * rr) * S]
            d.polygon(pts, outline=white, width=3 * S)
            d.ellipse(s(cx - r * 0.3, cy - r * 0.3, cx + r * 0.3, cy + r * 0.3), outline=white, width=3 * S)
        def f(d):
            gear(d, 60, 70, 40, 12)
            gear(d, 118, 112, 26, 9)
        overlay(img, f, 0.5)
    elif name == "terminal":
        def f(d):
            d.rounded_rectangle(s(18, 30, 142, 120), radius=6 * S, outline=white, width=3 * S)
            for i, w in enumerate((70, 96, 52, 84)):
                d.rectangle(s(30, 46 + i * 14, 30 + w, 50 + i * 14), fill=white)
            d.rectangle(s(30, 102, 40, 108), fill=white)
        overlay(img, f, 0.55)


def wrap(text, font, maxw):
    words = text.split()
    lines, cur = [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if font.getlength(t) <= maxw or not cur:
            cur = t
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines


def cover(title, top, bottom, motif_name):
    top, bottom = rgb(top), rgb(bottom)
    seed = int(hashlib.sha1(title.encode()).hexdigest()[:8], 16)
    rnd = random.Random(seed)
    img = gradient(top, bottom).convert("RGBA")
    motif(motif_name, img, top, bottom, rnd)

    # Unterer Bereich: dunkle Fläche als Grund für den Titel
    shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for y in range(150 * S, H * S):
        a = min(200, round((y - 150 * S) / (20 * S) * 200))
        sd.line([(0, y), (W * S, y)], fill=bottom + (a,))
    img.alpha_composite(shade)

    d = ImageDraw.Draw(img)
    size = 22
    while True:
        font = ImageFont.truetype(FONT, size * S)
        lines = wrap(title, font, (W - 24) * S)
        if (len(lines) <= 2 and all(font.getlength(l) <= (W - 24) * S for l in lines)) or size <= 14:
            break
        size -= 1
    lh = round(size * 1.15) * S
    y = (H - 14) * S - lh * len(lines)
    # Schatten weich, dann Schrift mit leichter Kontur (Inter liegt nur als Regular vor)
    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    shd = ImageDraw.Draw(sh)
    for i, line in enumerate(lines):
        shd.text((12 * S, y + i * lh + 2 * S), line, font=font, fill=(0, 0, 0, 150))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(3 * S)))
    for i, line in enumerate(lines):
        d.text((12 * S, y + i * lh), line, font=font, fill=(255, 255, 255, 255),
               stroke_width=max(1, S // 3), stroke_fill=(255, 255, 255, 255))
    # Akzentstrich über dem Titel
    d.rectangle([12 * S, y - 10 * S, 36 * S, y - 7 * S], fill=mix(top, (255, 255, 255), 0.4) + (255,))

    # dezenter Rahmen
    d.rectangle([0, 0, W * S - 1, H * S - 1], outline=(255, 255, 255, 40), width=2 * S)
    return img.resize((W, H), Image.LANCZOS).convert("RGB")


def main():
    for system, mock_rel, cover_rel, title, top, bottom, motif_name in GAMES:
        mock_path = os.path.join(MOCK, mock_rel)
        if not os.path.exists(mock_path):
            os.makedirs(os.path.dirname(mock_path), exist_ok=True)
            with open(mock_path, "wb") as f:
                f.write(MOCK_TEXT)
        out = os.path.join(SD, cover_rel)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        cover(title, top, bottom, motif_name).save(out, optimize=True)
        print(f"{system:5} {title:20} {os.path.relpath(out, ROOT)}")


if __name__ == "__main__":
    main()
