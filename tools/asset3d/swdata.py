"""Readers for the Syndicate Wars in-mission sprite and animation data.

Formats (see swrendersoft/include/frame_sprani.h and bflibrary bfsprite.h):
  mspr-N.tab  6 bytes per sprite: u32 offset into .dat, u8 width, u8 height
  mspr-N.dat  RLE pixels row by row: s8 0 = end of row, <0 = skip, >0 = copy
  nsta-0.ani  u16 animation start frames
  nfra-0.ani  8 bytes per frame: u16 FirstElement, u8 SWidth, SHeight, FX,
              Flags, u16 Next
  mele-0.ani  10 bytes per element: u16 ToSprite (byte offset in the tab),
              s16 X, s16 Y (half pixels), u16 Flags, u16 Next
  pal.pal     768 bytes, 6-bit RGB
"""
import os
import re
import struct

import numpy as np
from PIL import Image

PERSON_TYPES = [
    "none", "agent", "zealot", "punk_f", "briefcase_m", "white_brun_f",
    "mercenary", "mech_spider", "police", "punk_m", "scientist", "shady_m",
    "high_priest", "whit_blond_f", "leth_jackt_m", "fast_blond_f",
]

ANIM_NAMES = [
    "idle", "weplight_idle", "wepheavy_idle", "unkn03", "unkn04", "unkn05",
    "unkn06", "wepheavy_unkn07", "unkn08", "unkn09", "unkn10", "lay_down",
    "unkn12", "unkn13", "unkn14", "wepheavy_unkn15", "push_back", "unkn17",
    "dead_ash", "unkn19", "dead_body", "unkn21",
]

SPRITE_REC = 6


def find_gamedata(start=None):
    """Finds the gamedata folder: $SWARS_GAMEDATA, else a 'gamedata' folder
    next to one of the parents of this script."""
    env = os.environ.get("SWARS_GAMEDATA")
    if env:
        return env
    d = os.path.abspath(start or os.path.dirname(__file__))
    while True:
        cand = os.path.join(d, "gamedata")
        if os.path.isdir(os.path.join(cand, "data")):
            return cand
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("gamedata not found; set SWARS_GAMEDATA")
        d = parent


def find_repo_root():
    d = os.path.abspath(os.path.dirname(__file__))
    while not os.path.isfile(os.path.join(d, "src", "people.c")):
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("repo root (src/people.c) not found")
        d = parent
    return d


def load_palette(gamedata):
    raw = open(os.path.join(gamedata, "qdata", "pal.pal"), "rb").read()
    pal = np.frombuffer(raw[:768], dtype=np.uint8).reshape(256, 3).astype(np.int32)
    return np.clip(pal * 4 + pal // 16, 0, 255).astype(np.uint8)


def load_people_frames(repo_root):
    """Parses people_frames[16][22] from src/people.c."""
    src = open(os.path.join(repo_root, "src", "people.c"), encoding="latin-1").read()
    m = re.search(r"people_frames\s*\[[^\]]*\]\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit("people_frames not found in people.c")
    rows = re.findall(r"\{([^{}]*)\}", m.group(1))
    table = [[int(v) for v in r.replace(" ", "").split(",") if v] for r in rows]
    assert len(table) == 16 and all(len(r) == 22 for r in table), "unexpected people_frames shape"
    return table


class SpriteBank:
    def __init__(self, gamedata, variant=0):
        base = os.path.join(gamedata, "data")
        self.tab = open(os.path.join(base, "mspr-%d.tab" % variant), "rb").read()
        self.dat = open(os.path.join(base, "mspr-%d.dat" % variant), "rb").read()
        self.count = len(self.tab) // SPRITE_REC
        self._cache = {}

    def info(self, idx):
        return struct.unpack_from("<IBB", self.tab, idx * SPRITE_REC)

    def decode(self, idx):
        """Returns an (h, w) int16 array of palette indices, -1 = transparent."""
        if idx in self._cache:
            return self._cache[idx]
        off, w, h = self.info(idx)
        img = np.full((h, w), -1, dtype=np.int16)
        p = off
        dat = self.dat
        for y in range(h):
            x = 0
            while True:
                c = struct.unpack_from("b", dat, p)[0]
                p += 1
                if c == 0:
                    break
                if c < 0:
                    x -= c
                    continue
                row = dat[p:p + c]
                p += c
                n = max(0, min(c, w - x))
                if n > 0:
                    img[y, x:x + n] = np.frombuffer(row[:n], dtype=np.uint8)
                x += c
        self._cache[idx] = img
        return img


class AnimTables:
    def __init__(self, gamedata):
        base = os.path.join(gamedata, "data")
        nsta = open(os.path.join(base, "nsta-0.ani"), "rb").read()
        self.nstart = list(struct.unpack("<%dH" % (len(nsta) // 2), nsta))
        nfra = open(os.path.join(base, "nfra-0.ani"), "rb").read()
        self.frames = [struct.unpack_from("<HBBBBH", nfra, i * 8) for i in range(len(nfra) // 8)]
        mele = open(os.path.join(base, "mele-0.ani"), "rb").read()
        self.elements = [struct.unpack_from("<HhhHH", mele, i * 10) for i in range(len(mele) // 10)]

    def anim_frames(self, start_index, limit=64):
        """Frame numbers of the animation starting at nstart[start_index]."""
        first = self.nstart[start_index]
        out = []
        f = first
        while f and f not in out and len(out) < limit:
            out.append(f)
            f = self.frames[f][5]
        return out

    def frame_elements(self, frm, frv=(0, 0, 0, 0, 0)):
        """Elements drawn for a frame, filtered like draw_frame_unscaled_alpha_frv."""
        out = []
        el = self.frames[frm][0]
        seen = set()
        while el > 0 and el not in seen and el < len(self.elements):
            seen.add(el)
            to_spr, x, y, flags, nxt = self.elements[el]
            slot = (flags >> 4) & 0x1F
            ver = (flags >> 9) & 0x07
            want = frv[slot] if slot < len(frv) else 0
            if want == ver:
                out.append((to_spr // SPRITE_REC, x >> 1, y >> 1, flags & 0x07, slot))
            el = nxt
        return out


def compose_frame(bank, anims, frm, palette, frv=(0, 0, 0, 0, 0)):
    """Composes a frame into an RGBA image. Returns (image, (ox, oy)) where
    (ox, oy) is the position of the frame origin inside the image."""
    parts = []
    for spr, x, y, dflags, slot in anims.frame_elements(frm, frv):
        if spr <= 0 or spr >= bank.count:
            continue
        img = bank.decode(spr)
        if img.size == 0:
            continue
        if dflags & 1:
            img = img[:, ::-1]
        if dflags & 2:
            img = img[::-1, :]
        parts.append((img, x, y))
    if not parts:
        return None, (0, 0)
    x0 = min(x for _, x, _ in parts)
    y0 = min(y for _, _, y in parts)
    x1 = max(x + im.shape[1] for im, x, _ in parts)
    y1 = max(y + im.shape[0] for im, _, y in parts)
    canvas = np.full((y1 - y0, x1 - x0), -1, dtype=np.int16)
    for im, x, y in parts:
        sub = canvas[y - y0:y - y0 + im.shape[0], x - x0:x - x0 + im.shape[1]]
        mask = im >= 0
        sub[mask] = im[mask]
    return indices_to_rgba(canvas, palette), (-x0, -y0)


def indices_to_rgba(idx, palette):
    h, w = idx.shape
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    mask = idx >= 0
    rgba[mask, :3] = palette[idx[mask]]
    rgba[mask, 3] = 255
    return Image.fromarray(rgba, "RGBA")
