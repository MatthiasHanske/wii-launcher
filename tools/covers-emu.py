#!/usr/bin/env python3
"""Laedt Emulator-Cover (Libretro-Thumbnails) fuer N64, SNES, NES, Sega und Adventure (dos).

Aufruf: python3 -I tools/covers-emu.py [--dry-run] [--system n64|snes|nes|sega|dos]
                                       [--limit N] [--refresh] [--only TEILSTRING]
Ziel:   /Volumes/WII/launcher/covers/<systemKey>/<Dateiname ohne Endung>.png
        dos: /Volumes/WII/launcher/covers/dos/<Spielordner>.png (scummvm/ und dosbox/ auf der USB)

  --dry-run   nur Trefferbericht, nichts herunterladen
  --refresh   vorhandene Zieldateien neu laden und ueberschreiben (sonst uebersprungen)
  --only STR  nur ROMs/Spielordner, deren Name STR enthaelt (ohne Gross-/Kleinschreibung)

Cover werden auf hoechstens 320x228 verkleinert (Seitenverhaeltnis bleibt).
dos: ScummVM-Ordner suchen zuerst im Libretro-Repo "ScummVM", dann in "DOS"; dosbox-Ordner nur in "DOS".
Ordnernamen sind oft Kurzformen; dann trifft auch ein zusammenhaengender Teilstring des Libretro-Namens.
"""
import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request

USB = "/Volumes/WII-ROMS"
SD = "/Volumes/WII"
BACKUP = "/Users/matthias/Backups/WII-SD-2026-10-28"
CACHE = os.path.expanduser("~/.cache/wii-launcher-covers")
ORG = "libretro-thumbnails"
THRESHOLD = 0.85
# Die Launcher-Vorschau zeigt Cover im echten Seitenverhaeltnis: hochkant bis 160x228,
# querformatige Boxen (Sega, SNES-PAL u.a.) brauchen Breite, daher bis 320x228.
MAX_W, MAX_H = 320, 228
_warned_resize = False

# systemKey -> Liste von (Repo, Endungen)
SYSTEMS = {
    "n64": [("Nintendo_-_Nintendo_64", {"z64", "v64", "n64"})],
    "snes": [("Nintendo_-_Super_Nintendo_Entertainment_System", {"sfc", "smc", "fig", "swc"})],
    "nes": [("Nintendo_-_Nintendo_Entertainment_System", {"nes", "unf", "fds"})],
    "sega": [
        ("Sega_-_Mega_Drive_-_Genesis", {"md", "smd", "gen"}),
        ("Sega_-_Master_System_-_Mark_III", {"sms"}),
        ("Sega_-_SG-1000", {"sg"}),
    ],
}

# Ordner, die gescannt werden (relativ zu USB bzw. Backup)
USB_DIRS = ["not64/roms", "snes9xgx/roms", "fceugx/roms",
            "genplus/roms/md", "genplus/roms/sms", "genplus/roms/sg"]
BACKUP_DIRS = ["not64/roms", "snes9xgx/roms", "wiimednafen/roms/Master System",
               "wiimednafen/roms/Mega Drive"]
ADV_GLOBS = ["scummvm", "dosbox"]  # USB-Unterordner = je ein Spiel
ADV_BACKUP = ["dos"]
# Adventure-Unterordner -> Libretro-Repos in Suchreihenfolge
ADV_REPOS = {"scummvm": ["ScummVM", "DOS"], "dosbox": ["DOS"], "dos": ["DOS"]}
DOS_KEY = "dos"


class RateLimit(Exception):
    pass


def clean_title(name):
    """Entspricht dem cleanTitle des Launchers (ohne Endung uebergeben)."""
    s = re.sub(r"\[[^\]]*\]", " ", name)
    s = re.sub(r"\([^)]*\)", " ", s)  # Region/Sprache, Rev, V1.x, M5 ...
    s = s.replace("_", " ")
    s = re.sub(r"\s+", " ", s).strip()
    m = re.match(r"^(.*?),\s*(The|A|An)(\s.*|)$", s, re.I)
    if m:
        s = f"{m.group(2)} {m.group(1)}{m.group(3)}"
    return s.lower()


def norm(name):
    return re.sub(r"[^a-z0-9]", "", clean_title(name))


def http_get(url, accept=None):
    headers = {"User-Agent": "wii-launcher-covers"}
    if accept:
        headers["Accept"] = accept
    tok = os.environ.get("GITHUB_TOKEN")
    if tok and "api.github.com" in url:
        headers["Authorization"] = f"Bearer {tok}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def api_json(url):
    try:
        return json.loads(http_get(url, "application/vnd.github+json"))
    except urllib.error.HTTPError as e:
        if e.code in (403, 429) and (e.headers.get("X-RateLimit-Remaining") == "0" or e.code == 429):
            raise RateLimit(
                "GitHub-API-Limit erreicht. GITHUB_TOKEN setzen (z.B. export GITHUB_TOKEN=$(gh auth token)) "
                "oder spaeter erneut versuchen.")
        raise


def thumb_names(repo):
    """Liste der Named_Boxarts-Namen (ohne .png), mit Cache."""
    os.makedirs(CACHE, exist_ok=True)
    cache = os.path.join(CACHE, repo + ".json")
    if os.path.isfile(cache):
        with open(cache) as f:
            return json.load(f)
    names = None
    try:
        data = api_json(f"https://api.github.com/repos/{ORG}/{repo}/git/trees/master?recursive=1")
        if data.get("truncated"):
            raise ValueError("Baum abgeschnitten")
        pre = "Named_Boxarts/"
        names = [e["path"][len(pre):-4] for e in data["tree"]
                 if e["path"].startswith(pre) and e["path"].endswith(".png")]
    except RateLimit:
        raise
    except Exception as e:
        print(f"  API-Liste fuer {repo} fehlgeschlagen ({e}), versuche Unterbaum ...", file=sys.stderr)
        data = api_json(f"https://api.github.com/repos/{ORG}/{repo}/git/trees/master:Named_Boxarts")
        names = [e["path"][:-4] for e in data["tree"] if e["path"].endswith(".png")]
    if names:
        with open(cache + ".tmp", "w") as f:
            json.dump(names, f)
        os.replace(cache + ".tmp", cache)
    return names


def downscale(path):
    """Bild auf hoechstens MAX_W x MAX_H verkleinern (Seitenverhaeltnis bleibt).
    PIL, sonst sips (macOS), sonst unveraendert mit Warnung."""
    global _warned_resize
    try:
        from PIL import Image
        with Image.open(path) as im:
            if im.width <= MAX_W and im.height <= MAX_H:
                return
            im.thumbnail((MAX_W, MAX_H), Image.LANCZOS)
            im.save(path, "PNG")
        return
    except ImportError:
        pass
    if shutil.which("sips"):
        out = subprocess.run(["sips", "-g", "pixelWidth", "-g", "pixelHeight", path],
                             capture_output=True, text=True).stdout
        w = int(re.search(r"pixelWidth: (\d+)", out).group(1))
        h = int(re.search(r"pixelHeight: (\d+)", out).group(1))
        if w <= MAX_W and h <= MAX_H:
            return
        # sips -Z begrenzt die laengere Seite; so waehlen, dass beide Grenzen halten
        z = min(MAX_H if h >= w else MAX_W, int(max(w, h) * min(MAX_W / w, MAX_H / h)))
        subprocess.run(["sips", "-s", "format", "png", "-Z", str(z), path, "--out", path],
                       check=True, capture_output=True)
        return
    if not _warned_resize:
        print("Warnung: weder PIL noch sips vorhanden, Cover bleiben in Originalgroesse.",
              file=sys.stderr)
        _warned_resize = True


def region_rank(name):
    n = name.lower()
    for i, tag in enumerate(("(usa", "(world", "(europe", "(germany")):
        if tag in n:
            return i
    return 9


def suffix(k):
    """Abschliessende Zahl bzw. roemische Ziffer (Fortsetzungsnummer), sonst ''."""
    m = re.search(r"(\d+|(?:x{0,2}(?:ix|iv|v?i{0,3})))$", k)
    return m.group(1) if m and m.group(1) else ""


class Index:
    def __init__(self, names):
        self.by_norm = {}
        for n in sorted(names, key=lambda x: (region_rank(x), x)):
            self.by_norm.setdefault(norm(n), n)
        self.keys = list(self.by_norm)

    def match_contains(self, title_norm):
        """Normierter Titel als zusammenhaengender Teil eines Libretro-Namens (kuerzester gewinnt)."""
        if len(title_norm) < 6:
            return None, 0.0
        cands = [k for k in self.keys if title_norm in k]
        if not cands:
            return None, 0.0
        best = min(cands, key=lambda k: len(k))  # min ist stabil: Region-Rang bleibt Reihenfolge
        return self.by_norm[best], 0.9

    def match(self, title_norm):
        if not title_norm:
            return None, 0.0
        if title_norm in self.by_norm:
            return self.by_norm[title_norm], 1.0
        best, ratio = None, 0.0
        for k in self.keys:
            if abs(len(k) - len(title_norm)) > max(4, len(title_norm) * 0.2):
                continue
            sm = difflib.SequenceMatcher(None, title_norm, k)
            if sm.real_quick_ratio() < THRESHOLD or sm.quick_ratio() < THRESHOLD:
                continue
            if suffix(k) != suffix(title_norm):  # II vs III, 2 vs 3
                continue
            r = sm.ratio()
            if r > ratio:
                best, ratio = k, r
        if best and ratio >= THRESHOLD:
            return self.by_norm[best], ratio
        return None, ratio


def scan_roms(base, dirs):
    """-> Liste (systemKey, repo, stem, dateiname)"""
    out = []
    for d in dirs:
        p = os.path.join(base, d)
        if not os.path.isdir(p):
            continue
        for fn in sorted(os.listdir(p)):
            if fn.startswith("."):
                continue
            stem, _, ext = fn.rpartition(".")
            if not stem:
                continue
            ext = ext.lower()
            for key, repos in SYSTEMS.items():
                for repo, exts in repos:
                    if ext in exts:
                        out.append((key, repo, stem, fn))
    return out


def scan_adventure(base, subs):
    games = []
    for s in subs:
        p = os.path.join(base, s)
        if os.path.isdir(p):
            for fn in sorted(os.listdir(p)):
                if not fn.startswith(".") and os.path.isdir(os.path.join(p, fn)):
                    games.append((s, fn))
    return games


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true", help="nur Matching-Bericht, kein Download")
    ap.add_argument("--system", choices=sorted(SYSTEMS) + [DOS_KEY], help="nur dieses System")
    ap.add_argument("--limit", type=int, help="max. Anzahl ROMs je System")
    ap.add_argument("--refresh", action="store_true", help="vorhandene Cover neu laden und ueberschreiben")
    ap.add_argument("--only", metavar="TEILSTRING", help="nur ROMs, deren Dateiname den Teilstring enthaelt")
    a = ap.parse_args()

    use_usb = os.path.isdir(USB)
    if use_usb:
        base, dirs, adv_base, adv = USB, USB_DIRS, USB, ADV_GLOBS
        print(f"Quelle: {USB}")
    else:
        if not a.dry_run:
            print(f"{USB} nicht gemountet; ohne --dry-run nicht sinnvoll.", file=sys.stderr)
            return 1
        base, dirs, adv_base, adv = BACKUP, BACKUP_DIRS, BACKUP, ADV_BACKUP
        print(f"Quelle: Backup {BACKUP} (USB nicht gemountet)")
    if not a.dry_run and not os.path.isdir(SD):
        print(f"{SD} nicht gemountet.", file=sys.stderr)
        return 1

    only = a.only.lower() if a.only else None
    # Jobs: (key, [repos], stem, dateiname)
    jobs = []
    for key, repo, stem, fn in scan_roms(base, dirs):
        jobs.append((key, [repo], stem, fn))
    for sub, folder in scan_adventure(adv_base, adv):
        jobs.append((DOS_KEY, ADV_REPOS.get(sub, ["DOS"]), folder, f"{sub}/{folder}"))
    indexes = {}
    stats = {}
    downloaded = skipped = failed = 0
    per_sys_count = {}

    def index_for(repo):
        if repo not in indexes:
            print(f"Lade Namensliste {repo} ...")
            indexes[repo] = Index(thumb_names(repo))
        return indexes[repo]

    try:
        for key, repos, stem, fn in jobs:
            if a.system and key != a.system:
                continue
            if only and only not in fn.lower():
                continue
            if a.limit is not None and per_sys_count.get(key, 0) >= a.limit:
                continue
            per_sys_count[key] = per_sys_count.get(key, 0) + 1
            st = stats.setdefault(key, {"match": [], "miss": []})
            n = norm(stem)
            name, ratio, repo = None, 0.0, None
            for repo in repos:
                idx = index_for(repo)
                name, ratio = idx.match(n)
                if not name and key == "n64" and n.endswith("64") and len(n) > 6:
                    # Libretro fuehrt N64-Titel ohne "64"-Zusatz ("Quake 64" -> "Quake")
                    name, ratio = idx.match(n[:-2])
                    ratio = min(ratio, 0.99)
                if not name and key == DOS_KEY:
                    name, ratio = idx.match_contains(n)
                if name:
                    break
            if not name:
                st["miss"].append(stem)
                continue
            st["match"].append((stem, name, ratio))
            if a.dry_run:
                continue
            dest = os.path.join(SD, "launcher", "covers", key, stem + ".png")
            if os.path.isfile(dest) and not a.refresh:
                skipped += 1
                continue
            url = (f"https://raw.githubusercontent.com/{ORG}/{repo}/master/Named_Boxarts/"
                   + urllib.parse.quote(name) + ".png")
            try:
                data = http_get(url)
                if not data.startswith(b"\x89PNG\r\n\x1a\n"):
                    raise ValueError("keine PNG-Datei")
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with open(dest + ".tmp", "wb") as f:
                    f.write(data)
                downscale(dest + ".tmp")
                os.replace(dest + ".tmp", dest)
                downloaded += 1
            except Exception as e:
                failed += 1
                st["miss"].append(f"{stem} (Download: {e})")
    except RateLimit as e:
        print(str(e), file=sys.stderr)
        return 2
    except urllib.error.HTTPError as e:
        hint = " (vermutlich Rate-Limit, GITHUB_TOKEN setzen)" if e.code == 403 else ""
        print(f"HTTP-Fehler {e.code}{hint}: {e.url}", file=sys.stderr)
        return 2
    except urllib.error.URLError as e:
        print(f"Netzwerkfehler: {e.reason}", file=sys.stderr)
        return 2

    print("\n== Bericht ==")
    for key in list(SYSTEMS) + [DOS_KEY]:
        if key not in stats:
            continue
        st = stats[key]
        total = len(st["match"]) + len(st["miss"])
        titles = {}
        fuzzy = {}
        for stem, name, r in st["match"]:
            titles.setdefault(name, []).append(stem)
            if r < 1.0:
                fuzzy[name] = True
        print(f"\n[{key}] Treffer {len(st['match'])}/{total}, fehlend {len(st['miss'])}")
        for name, stems in titles.items():
            mark = "~" if fuzzy.get(name) else "+"
            print(f"  {mark} {name}  <-  {len(stems)}x ({stems[0]}{' ...' if len(stems) > 1 else ''})")
        for m in st["miss"]:
            print(f"  - {m}")
    if not a.dry_run:
        print(f"\nHeruntergeladen {downloaded}, vorhanden {skipped}, Fehler {failed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
