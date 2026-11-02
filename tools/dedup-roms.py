#!/usr/bin/env python3
"""Dedup der Wii-Launcher-ROMs. Bericht: python3 -I dedup.py --root /Volumes/WII-ROMS
Loeschen: ... --apply  (fuehrt delete.txt-Logik aus, os.remove, mit Protokoll)"""
import argparse, os, re, sys, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- Port von titles.cpp ----
REGION_WORDS = {"europe","germany","usa","japan","world","australia","scandinavia","france","spain","italy","netherlands","uk","korea","brazil"}
CODES = {"u","e","j","w","ue","ju","jue","g","f","i","s","pal","ntsc"}
FLAGS = {"digital","aftermarket","pirate","unl","!"}
LANGS = {"en","fr","de","es","it","nl","pt","sv","da","no","fi","ja","ko","zh","ru","pl","el","tr","cs","hu"}
ARTICLES = ["The","Die","Der","Das"]

def lower_ascii(s):
    return "".join(chr(ord(c)+32) if "A" <= c <= "Z" else c for c in s)
def trim(s): return s.strip(" \t")
def collapse(s):
    out = []; space = False
    for c in s:
        if c in " \t": space = True; continue
        if space and out: out.append(" ")
        space = False; out.append(c)
    return "".join(out)

def is_tag_content(raw):
    s = lower_ascii(trim(raw))
    if not s: return False
    if s in CODES or s in FLAGS: return True
    if s.startswith("rev ") or s.startswith("rev-") or s == "rev": return True
    if len(s) >= 2 and s[0] in "vm" and s[1].isdigit(): return True
    parts = [trim(p) for p in s.split(",")]
    all_region = all(p in REGION_WORDS for p in parts)
    all_lang = all(p in LANGS for p in parts)
    return all_region or (all_lang and len(parts) >= 2)

def strip_groups(s):
    out = []; i = 0
    while i < len(s):
        c = s[i]
        if c in "[(":
            close = "]" if c == "[" else ")"
            e = s.find(close, i+1)
            if e != -1 and (c == "[" or is_tag_content(s[i+1:e])):
                i = e + 1; out.append(" "); continue
        out.append(c); i += 1
    return "".join(out)

def move_article(s):
    for art in ARTICLES:
        needle = ", " + art
        pos = 0
        while True:
            pos = s.find(needle, pos)
            if pos == -1: break
            after = pos + len(needle)
            if after == len(s) or s.startswith(" - ", after):
                return art + " " + s[:pos] + s[after:]
            pos = after
    return s

def clean_title(raw):
    s = raw.replace("_", " ")
    s = collapse(strip_groups(s))
    s = collapse(move_article(s))
    while len(s) >= 1 and s[-1] == "-" and (len(s) == 1 or s[-2] == " "):
        s = trim(s[:-1])
    return s

C3 = {}
def _reg(res, bytes_):
    for b in bytes_: C3[b] = res
_reg("a", list(range(0x80,0x86))+list(range(0xA0,0xA6)))
_reg("c",[0x87,0xA7]); _reg("e",[0x88,0x89,0x8A,0x8B,0xA8,0xA9,0xAA,0xAB])
_reg("i",[0x8C,0x8D,0x8E,0x8F,0xAC,0xAD,0xAE,0xAF]); _reg("n",[0x91,0xB1])
_reg("o",[0x92,0x93,0x94,0x95,0x96,0xB2,0xB3,0xB4,0xB5,0xB6]); _reg("u",[0x99,0x9A,0x9B,0x9C,0xB9,0xBA,0xBB,0xBC]); _reg("ss",[0x9F])

def fold_ascii(s):
    # Arbeitet wie C++ auf UTF-8-Bytes
    b = s.encode("utf-8", "surrogateescape"); out = []; i = 0
    while i < len(b):
        c = b[i]
        if c < 0x80:
            i += 1
            ch = chr(c)
            if "A" <= ch <= "Z": out.append(chr(c+32))
            elif "a" <= ch <= "z" or "0" <= ch <= "9": out.append(ch)
            elif ch not in "'`": out.append(" ")
            continue
        n = 4 if 0xF0 <= c < 0xF8 else 3 if c >= 0xE0 else 2 if c >= 0xC0 else 1
        n = min(n, len(b)-i)
        if n == 2 and c == 0xC3: out.append(C3.get(b[i+1], " "))
        elif n == 3 and c == 0xE2 and b[i+1] == 0x80 and b[i+2] in (0x98, 0x99): pass
        else: out.append(" ")
        i += n
    return "".join(out)

def sort_key(title):
    s = lower_ascii(trim(title))
    for a in ("the ","die ","der ","das "):
        if s.startswith(a): s = s[4:]; break
    return "".join(c for c in fold_ascii(s) if c != " ")

# ---- Systeme ----
SYSTEMS = [
    ("N64", "not64/roms", {"z64","n64","v64"}),
    ("SNES", "snes9xgx/roms", {"sfc","smc","zip"}),
    ("NES", "fceugx/roms", {"nes","unf","zip"}),
    ("Mega Drive", "genplus/roms/md", {"gen","md","smd","bin","zip"}),
    ("Master System", "genplus/roms/sms", {"sms","zip"}),
    ("SG-1000", "genplus/roms/sg", {"sg","zip"}),
]

EU = {"e","europe","eu","ue","g","germany","de","f","pal"}
WORLD = {"w","world"}
US = {"u","usa","ju"}
JP = {"j","japan"}

def region_rank(name):
    best = 4
    for grp in re.findall(r"\(([^)]*)\)", name):
        for p in grp.lower().split(","):
            p = p.strip()
            r = 0 if p in EU else 1 if p in WORLD else 2 if p in US else 3 if p in JP else 4
            best = min(best, r)
    for t in re.findall(r"\[([^\]]*)\]", name):
        t = t.lower()
        if t in ("e", "ue"): best = min(best, 0)
        elif t in ("w",): best = min(best, 1)
        elif t in ("u", "ju"): best = min(best, 2)
        elif t == "j": best = min(best, 3)
    return best

def quality_rank(name):
    tags = [t.lower() for t in re.findall(r"\[([^\]]*)\]", name)]
    rank = 1; has_ok = False
    for t in tags:
        if t == "!": has_ok = True; continue
        c = t[:1]
        r = {"a":2,"f":3,"o":4,"b":5}.get(c, 6 if c in "htp" else 1)
        rank = max(rank, r)
    if rank == 1 and has_ok: rank = 0
    return rank

def is_bad(name): return quality_rank(name) >= 5

def region_label(r): return ["EU","World","US","JP","other"][r]

def scan(root):
    res = {}
    for sysname, rel, exts in SYSTEMS:
        d = os.path.join(root, rel)
        files, junk = [], []
        if os.path.isdir(d):
            for n in sorted(os.listdir(d)):
                p = os.path.join(d, n)
                if os.path.isdir(p): continue
                if n.startswith("._") or n == ".DS_Store": junk.append((n, os.path.getsize(p))); continue
                base, dot, ext = n.rpartition(".")
                if not dot or not base: continue
                if ext.lower() not in exts: continue
                files.append((n, base, os.path.getsize(p)))
        groups = {}
        for n, base, size in files:
            groups.setdefault(sort_key(clean_title(base)), []).append((n, base, size))
        dups = []
        for k, g in groups.items():
            if len(g) < 2: continue
            g.sort(key=lambda f: (region_rank(f[1]), quality_rank(f[1]), -f[2], f[0]))
            title = clean_title(g[0][1])
            dups.append((title, k, g))
        dups.sort(key=lambda x: x[1])
        res[sysname] = dict(dir=d, files=files, junk=junk, dups=dups)
    return res

def human(n):
    return f"{n/1048576:.1f} MiB"

def table(dups, d):
    lines = ["| Titel | behalten | löschen |", "|---|---|---|"]
    for title, k, g in dups:
        keep = g[0]
        def lab(f): return f"{f[0]} ({human(f[2])})".replace("|", "\\|")
        notes = ""
        lines.append(f"| {title.replace('|','\\|')} | {lab(keep)}{notes} | " + "<br>".join(lab(f) for f in g[1:]) + " |")
    return lines

def notes_for(sysname, dups):
    out = []
    if sysname == "N64":
        for title, k, g in dups:
            ok = [f for f in g if "[!]" in f[0]]
            if ok and all(region_rank(f[1]) == 2 for f in ok) and region_rank(g[0][1]) == 0 and "[!]" not in g[0][0]:
                out.append(f"- {title}: nur US-Fassung hat [!] ({', '.join(f[0] for f in ok)}), behalten wird trotzdem {g[0][0]} (EU)")
            if region_rank(g[0][1]) == 2 and any(region_rank(f[1]) == 2 and "[!]" in f[0] for f in g):
                out.append(f"- {title}: behalten wird US-Fassung {g[0][0]} (keine EU/Welt-Fassung)")
    return out

def extra_checks(root, res):
    out = ["## Sonderprüfungen", ""]
    d = res["N64"]["dir"]
    out.append("### (a) N64 \"The New Tetris\"")
    for n, base, size in res["N64"]["files"]:
        if sort_key(clean_title(base)) == sort_key("The New Tetris"):
            out.append(f"- `{n}` - {size} Bytes ({human(size)})")
    out.append("")
    out.append("### (b) SNES Zelda / DuckTales")
    sd = res["SNES"]["dir"]
    for n, base, size in res["SNES"]["files"]:
        low = base.lower()
        if "zelda" not in low and "ducktales" not in low and "duck tales" not in low: continue
        line = f"- `{n}` - {size} Bytes ({human(size)})"
        p = os.path.join(sd, n)
        if n.lower().endswith(".zip"):
            r = subprocess.run(["unzip", "-l", p], capture_output=True, text=True)
            line += "\n  ```\n" + "\n".join("  " + l for l in r.stdout.splitlines()) + "\n  ```"
        else:
            with open(p, "rb") as fh: b2 = fh.read(2)
            line += f", erste 2 Bytes: {b2.hex()}, size mod 1024 = {size % 1024}" + (" -> 512-Byte-Copier-Header wahrscheinlich" if size % 1024 == 512 else " -> kein Header-Hinweis")
        out.append(line)
    return out

def build(root, res):
    delete = []
    main = ["# ROM-Dubletten Bericht", "", f"Root: {root}", ""]
    main += ["| System | Dateien | Gruppen mit Doppelten | zu löschen | Größe | Junk (._/.DS_Store) |", "|---|---|---|---|---|---|"]
    detail = []; sep = {}
    for sysname, r in res.items():
        dl = [f for _, _, g in r["dups"] for f in g[1:]]
        size = sum(f[2] for f in dl)
        for f in dl: delete.append(os.path.join(r["dir"], f[0]))
        junk = len(r["junk"]); jsize = sum(s for _, s in r["junk"])
        main.append(f"| {sysname} | {len(r['files'])} | {len(r['dups'])} | {len(dl)} | {human(size)} | {junk} ({human(jsize)}) |")
        lines = [f"## {sysname} ({r['dir']})", ""]
        lines += notes_for(sysname, r["dups"]) and ["Hinweise:"] + notes_for(sysname, r["dups"]) + [""] or []
        if sysname in ("NES", "Mega Drive") and len(r["dups"]) > 40:
            fn = "report-" + sysname.lower().replace(" ", "") + ".md"
            sep[fn] = [f"# {sysname}: alle {len(r['dups'])} Gruppen", ""] + table(r["dups"], r["dir"])
            lines += [f"Vollständige Tabelle: {fn}. Stichprobe (erste 15 von {len(r['dups'])}):", ""] + table(r["dups"][:15], r["dir"])
        else:
            lines += table(r["dups"], r["dir"]) if r["dups"] else ["Keine Doppelten."]
        lines.append("")
        if r["junk"]:
            lines += ["Junk-Dateien (nicht in Dubletten, nicht in delete.txt): " + ", ".join(n for n, _ in r["junk"][:20]) + (" ..." if len(r["junk"]) > 20 else ""), ""]
        detail += lines
    main += [""] + detail + extra_checks(root, res)
    return "\n".join(main) + "\n", sep, delete

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--out", default=HERE)
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    res = scan(a.root)
    rep, sep, delete = build(a.root, res)
    if a.apply:
        # Liste neu berechnen und nur Dateien unterhalb root loeschen
        n_ok = n_miss = 0
        for p in delete:
            if not p.startswith(os.path.abspath(a.root) + os.sep): print("SKIP (ausserhalb root):", p); continue
            if not os.path.isfile(p): print("FEHLT:", p); n_miss += 1; continue
            try:
                os.remove(p); print("GELOESCHT:", p); n_ok += 1
            except OSError as e:
                print("FEHLER:", p, e)
        print(f"{n_ok} geloescht, {n_miss} fehlten")
        return
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "report.md"), "w").write(rep)
    for fn, lines in sep.items(): open(os.path.join(a.out, fn), "w").write("\n".join(lines) + "\n")
    open(os.path.join(a.out, "delete.txt"), "w").write("".join(p + "\n" for p in delete))
    print(rep.split("\n\n")[2] if False else "\n".join(rep.splitlines()[4:12]))

if __name__ == "__main__":
    main()
