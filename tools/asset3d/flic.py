"""Decoder for the game's FLIC animations (e.g. data/equip/wep-NN.fli).

Bullfrog's files use a 12-byte header (u32 size, u16 magic 0xAF12, u16
frames, u16 width, u16 height) instead of the standard 128 bytes; frame
chunks (0xF1FA) follow directly. Chunk types as in bflibrary gflicply.c.

python flic.py <file.fli> <outdir> [palette.pal]   writes frame PNGs and a contact sheet.
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

FRAME = 0xF1FA
COLOUR256, SS2, COLOUR, LC, BLACK, BRUN, COPY, PSTAMP = 4, 7, 11, 12, 13, 15, 16, 18


def decode(path, palette_file=None):
    d = open(path, "rb").read()
    size, magic, nframes, w, h = struct.unpack_from("<IHHHH", d, 0)
    if magic not in (0xAF11, 0xAF12):
        raise ValueError("not a FLIC: %x" % magic)
    p = 12
    if struct.unpack_from("<H", d, 16)[0] != FRAME:
        p = 128  # standard header
    pal = np.zeros((256, 3), np.uint8)
    if palette_file:  # 768 bytes of 6-bit RGB, for files without a palette chunk
        raw = np.frombuffer(open(palette_file, "rb").read()[:768], np.uint8).reshape(256, 3)
        pal[:] = np.clip(raw.astype(np.int32) * 4 + raw // 16, 0, 255)
    img = np.zeros((h, w), np.uint8)
    frames = []
    while p + 16 <= len(d) and len(frames) < nframes + 1:
        fsize, ftype, nch = struct.unpack_from("<IHH", d, p)
        if fsize == 0:
            break
        if ftype == FRAME:
            q = p + 16
            for _ in range(nch):
                csize, ctype = struct.unpack_from("<IH", d, q)
                body = q + 6
                if ctype in (COLOUR256, COLOUR):
                    _palette(d, body, pal, 4 if ctype == COLOUR else 1)
                elif ctype == BRUN:
                    _brun(d, body, img)
                elif ctype == LC:
                    _lc(d, body, img)
                elif ctype == SS2:
                    _ss2(d, body, img)
                elif ctype == BLACK:
                    img[:] = 0
                elif ctype == COPY:
                    img[:] = np.frombuffer(d, np.uint8, w * h, body).reshape(h, w)
                q += csize
            frames.append(Image.fromarray(pal[img], "RGB"))
        p += fsize
    return frames[:nframes]


def _palette(d, p, pal, mul):
    npk = struct.unpack_from("<H", d, p)[0]
    p += 2
    idx = 0
    for _ in range(npk):
        idx += d[p]
        n = d[p + 1] or 256
        p += 2
        for _ in range(n):
            pal[idx % 256] = [min(255, c * mul) for c in d[p:p + 3]]
            p += 3
            idx += 1


def _brun(d, p, img):
    h, w = img.shape
    for y in range(h):
        p += 1  # packet count (unused)
        x = 0
        while x < w:
            c = struct.unpack_from("b", d, p)[0]
            p += 1
            if c > 0:
                img[y, x:x + c] = d[p]
                p += 1
                x += c
            else:
                n = -c
                img[y, x:x + n] = np.frombuffer(d, np.uint8, n, p)
                p += n
                x += n


def _lc(d, p, img):
    y0, nl = struct.unpack_from("<HH", d, p)
    p += 4
    for y in range(y0, y0 + nl):
        npk = d[p]
        p += 1
        x = 0
        for _ in range(npk):
            x += d[p]
            c = struct.unpack_from("b", d, p + 1)[0]
            p += 2
            if c > 0:
                img[y, x:x + c] = np.frombuffer(d, np.uint8, c, p)
                p += c
                x += c
            else:
                img[y, x:x - c] = d[p]
                p += 1
                x -= c


def _ss2(d, p, img):
    h, w = img.shape
    nlines = struct.unpack_from("<H", d, p)[0]
    p += 2
    y = 0
    done = 0
    while done < nlines:
        word = struct.unpack_from("<H", d, p)[0]
        p += 2
        if word & 0xC000 == 0xC000:
            y += 0x10000 - word
            continue
        if word & 0xC000 == 0x8000:
            img[y, w - 1] = word & 0xFF
            continue
        x = 0
        for _ in range(word):
            x += d[p]
            c = struct.unpack_from("b", d, p + 1)[0]
            p += 2
            if c > 0:
                n = 2 * c
                img[y, x:x + n] = np.frombuffer(d, np.uint8, n, p)
                p += n
                x += n
            else:
                img[y, x:x - 2 * c] = np.tile(np.frombuffer(d, np.uint8, 2, p), -c)
                p += 2
                x -= 2 * c
        y += 1
        done += 1


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    frames = decode(src, sys.argv[3] if len(sys.argv) > 3 else None)
    base = os.path.splitext(os.path.basename(src))[0]
    for i, f in enumerate(frames):
        f.save(os.path.join(out, "%s_%03d.png" % (base, i)))
    w, h = frames[0].size
    cols = 6
    rows = (len(frames) + cols - 1) // cols
    sheet = Image.new("RGB", (w * cols, h * rows))
    for i, f in enumerate(frames):
        sheet.paste(f, ((i % cols) * w, (i // cols) * h))
    sheet.save(os.path.join(out, base + "_sheet.png"))
    print("%s: %d frames %dx%d" % (base, len(frames), w, h))


if __name__ == "__main__":
    main()
