#!/usr/bin/env python3
"""
Pikachu Volleyball - PSP asset converter.

Converts the original web-port assets into PSP-usable formats:

  1. sprite_sheet.png (476x885 RGBA atlas)  ->  repacked 512x512 tiles,
     stored as GU_PSM_5551 (RGBA 5-5-5-1, 16-bit).  PSP textures are limited
     to 512x512, so the original atlas is bin-packed (shelf packing) into as
     many 512x512 tiles as required (2 for the stock assets).

     Output:
       assets/sprite_sheet_0.raw, sprite_sheet_1.raw, ...
       src/atlas.h   (frame table: tile index + pixel rect for every frame)

  2. The 7 WAV SFX (8-bit mono 22050) -> 16-bit mono 44100 PCM, packed into
     a single file with a small header.

     Output:
       assets/sfx.bin
       src/audio_layout.h (per-sfx offset + sample count)

  3. bgm.mp3 -> 16-bit mono 44100 PCM (via mpg123 or ffmpeg), padded to a
     multiple of 2048 samples for chunked streaming.

     Output:
       assets/bgm.raw
       src/audio_layout.h (BGM total sample count)

Dependencies: Python 3, Pillow, and either `mpg123` or `ffmpeg` (for BGM).
The SFX + atlas conversion only needs Pillow.

Usage:
    python3 tools/convert_assets.py [--src DIR] [--out DIR]
"""

import json
import os
import struct
import subprocess
import sys
import wave

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # PSP-PORT/

SRC_DIR = os.path.abspath(os.path.join(
    ROOT, os.pardir, "pikachu-volleyball", "src", "resources", "assets"))
OUT_ASSETS = os.path.join(ROOT, "assets")
OUT_SRC = os.path.join(ROOT, "src")

SPRITE_SHEET = os.path.join(SRC_DIR, "images", "sprite_sheet.png")
SPRITE_JSON = os.path.join(SRC_DIR, "images", "sprite_sheet.json")
SOUNDS_DIR = os.path.join(SRC_DIR, "sounds")
BGM = os.path.join(SOUNDS_DIR, "bgm.mp3")

SFX_FILES = [
    ("pipikachu", "WAVE140_1.wav"),
    ("pika", "WAVE141_1.wav"),
    ("chu", "WAVE142_1.wav"),
    ("pi", "WAVE143_1.wav"),
    ("pikachu", "WAVE144_1.wav"),
    ("powerhit", "WAVE145_1.wav"),
    ("balltouchesground", "WAVE146_1.wav"),
]

TILE = 512
ATLAS_FMT = "5551"  # GU_PSM_5551


def err(msg):
    print("ERROR: " + msg, file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Atlas conversion
# ---------------------------------------------------------------------------

def rgb888_to_5551(r, g, b, a):
    """Pack an RGBA8888 pixel into GU_PSM_5551 (ABGR 1555 little-endian)."""
    a_bit = 1 if a >= 128 else 0
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return (a_bit << 15) | (b5 << 10) | (g5 << 5) | r5


def shelf_pack(frames):
    """Pack (w, h) rects into 512x512 sheets. Returns list of (sheet, x, y)."""
    # frames: list of dicts with 'w', 'h' (already sorted by caller)
    sheets = []
    placements = []  # parallel to frames
    for f in frames:
        w, h = f["w"], f["h"]
        placed = False
        # try existing sheets
        for s in sheets:
            if s["x"] + w <= TILE and s["y"] + h <= TILE:
                placements.append((s["index"], s["x"], s["y"]))
                s["x"] += w
                s["row_h"] = max(s["row_h"], h)
                placed = True
                break
            # can't fit on this row -> advance cursor to next row first
            if s["x"] > 0:
                s["y"] += s["row_h"]
                s["x"] = 0
                s["row_h"] = 0
                if s["x"] + w <= TILE and s["y"] + h <= TILE:
                    placements.append((s["index"], s["x"], s["y"]))
                    s["x"] += w
                    s["row_h"] = max(s["row_h"], h)
                    placed = True
                    break
        if not placed:
            # start a new sheet
            idx = len(sheets)
            sheets.append({"index": idx, "x": 0, "y": 0, "row_h": 0})
            s = sheets[-1]
            if w > TILE or h > TILE:
                err("frame %dx%d exceeds tile size" % (w, h))
            placements.append((idx, 0, 0))
            s["x"] = w
            s["row_h"] = h
    return sheets, placements


def convert_atlas():
    from PIL import Image

    img = Image.open(SPRITE_SHEET).convert("RGBA")
    W, H = img.size
    px = img.load()

    data = json.load(open(SPRITE_JSON))
    frames_meta = data["frames"]

    # Build sorted list of (key, rect)
    keys = sorted(frames_meta.keys())
    rects = []
    for k in keys:
        r = frames_meta[k]["frame"]
        rects.append((k, r["x"], r["y"], r["w"], r["h"]))

    # Sort frames by height desc for better shelf packing
    order = sorted(range(len(rects)), key=lambda i: -rects[i][4])
    packed = [None] * len(rects)
    shelf_frames = [{"w": rects[i][3], "h": rects[i][4]} for i in order]
    sheets, placements = shelf_pack(shelf_frames)
    for j, i in enumerate(order):
        packed[i] = placements[j]  # (sheet, x, y)

    num_sheets = len(sheets)

    # Render each sheet
    for si in range(num_sheets):
        out = bytearray(TILE * TILE * 2)
        for i, (k, fx, fy, fw, fh) in enumerate(rects):
            sheet, ox, oy = packed[i]
            if sheet != si:
                continue
            for yy in range(fh):
                for xx in range(fw):
                    r, g, b, a = px[fx + xx, fy + yy]
                    v = rgb888_to_5551(r, g, b, a)
                    off = ((oy + yy) * TILE + (ox + xx)) * 2
                    out[off] = v & 0xFF
                    out[off + 1] = (v >> 8) & 0xFF
        path = os.path.join(OUT_ASSETS, "sprite_sheet_%d.raw" % si)
        with open(path, "wb") as f:
            f.write(out)
        print("wrote %s (%d bytes)" % (path, len(out)))

    # Generate atlas.h
    lines = []
    lines.append("/* Auto-generated by tools/convert_assets.py -- do not edit. */")
    lines.append("#ifndef PIKA_ATLAS_H")
    lines.append("#define PIKA_ATLAS_H")
    lines.append("")
    lines.append("#define ATLAS_TILE_SIZE 512")
    lines.append("#define ATLAS_NUM_TILES %d" % num_sheets)
    lines.append("#define ATLAS_FORMAT GU_PSM_5551")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    int tile;   /* which 512x512 sheet */")
    lines.append("    int x, y;   /* top-left pixel in sheet */")
    lines.append("    int w, h;   /* size in pixels */")
    lines.append("} AtlasFrame;")
    lines.append("")
    lines.append("enum {")
    for k in keys:
        ident = "TEX_" + k[:-4].replace("/", "_").replace(".", "_").upper()
        lines.append("    %s," % ident)
    lines.append("    TEX_COUNT,")
    lines.append("};")
    lines.append("")
    lines.append("static const AtlasFrame g_atlas_frames[TEX_COUNT] = {")
    for i, (k, fx, fy, fw, fh) in enumerate(rects):
        sheet, ox, oy = packed[i]
        ident = "TEX_" + k[:-4].replace("/", "_").replace(".", "_").upper()
        lines.append("    [%s] = { %d, %d, %d, %d, %d },  /* %s */"
                     % (ident, sheet, ox, oy, fw, fh, k))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* PIKA_ATLAS_H */")
    with open(os.path.join(OUT_SRC, "atlas.h"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s (%d frames, %d sheets)" % (
        os.path.join(OUT_SRC, "atlas.h"), len(keys), num_sheets))


# ---------------------------------------------------------------------------
# Audio conversion
# ---------------------------------------------------------------------------

def wav_8bit_mono_to_s16(path):
    """Read an 8-bit mono WAV, return list of 16-bit samples (native rate)."""
    w = wave.open(path, "rb")
    if w.getsampwidth() != 1 or w.getnchannels() != 1:
        err("unexpected WAV format in %s (need 8-bit mono)" % path)
    rate = w.getframerate()
    raw = w.readframes(w.getnframes())
    w.close()
    # 8-bit PCM is unsigned with 128 == silence
    samples = [(b - 128) << 8 for b in raw]  # range -32768..32512
    return samples, rate


def upsample_2x(samples):
    """Linear-interpolate 2x (22050 -> 44100)."""
    n = len(samples)
    out = []
    for i in range(n):
        out.append(samples[i])
        nxt = samples[i + 1] if i + 1 < n else samples[i]
        out.append((samples[i] + nxt) >> 1)
    return out


def convert_sfx():
    entries = []
    blobs = []
    offset = 0
    # header: magic(4) + count(4), then count * (offset(4) + nsamples(4))
    hdr_size = 8 + len(SFX_FILES) * 8
    offset = hdr_size
    for name, fname in SFX_FILES:
        path = os.path.join(SOUNDS_DIR, fname)
        samples, rate = wav_8bit_mono_to_s16(path)
        if rate == 22050:
            samples = upsample_2x(samples)
        elif rate != 44100:
            err("unexpected SFX rate %d in %s" % (rate, path))
        data = struct.pack("<%dh" % len(samples), *samples)
        entries.append((name, offset, len(samples)))
        blobs.append(data)
        offset += len(data)

    with open(os.path.join(OUT_ASSETS, "sfx.bin"), "wb") as f:
        f.write(struct.pack("<4sI", b"PKSF", len(SFX_FILES)))
        for name, off, ns in entries:
            f.write(struct.pack("<II", off, ns))
        for b in blobs:
            f.write(b)
    print("wrote %s (%d bytes)" % (
        os.path.join(OUT_ASSETS, "sfx.bin"), offset))

    # audio_layout.h
    lines = []
    lines.append("/* Auto-generated by tools/convert_assets.py -- do not edit. */")
    lines.append("#ifndef PIKA_AUDIO_LAYOUT_H")
    lines.append("#define PIKA_AUDIO_LAYOUT_H")
    lines.append("")
    lines.append("enum {")
    for i, (name, off, ns) in enumerate(entries):
        lines.append("    SFX_%s = %d," % (name.upper(), i))
    lines.append("    SFX_COUNT,")
    lines.append("};")
    lines.append("")
    lines.append("typedef struct { unsigned int offset; unsigned int nsamples; } SfxInfo;")
    lines.append("static const SfxInfo g_sfx_info[SFX_COUNT] = {")
    for i, (name, off, ns) in enumerate(entries):
        lines.append("    { %d, %d },  /* %s */" % (off, ns, name))
    lines.append("};")
    lines.append("")
    lines.append("/* BGM total sample count (mono 16-bit 44100) - patched below. */")
    lines.append("#define BGM_TOTAL_SAMPLES 0")
    lines.append("")
    lines.append("#endif /* PIKA_AUDIO_LAYOUT_H */")
    with open(os.path.join(OUT_SRC, "audio_layout.h"), "w") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote %s" % os.path.join(OUT_SRC, "audio_layout.h"))
    return entries


def decode_bgm():
    """Decode bgm.mp3 to 16-bit mono 44100 raw PCM. Returns list of samples."""
    tmp_wav = os.path.join(OUT_ASSETS, "_bgm_tmp.wav")
    raw_path = os.path.join(OUT_ASSETS, "bgm.raw")

    # Try mpg123 first, then ffmpeg.
    cmd = None
    if shutil_which("mpg123"):
        cmd = ["mpg123", "-q", "-m", "-r", "44100", "-w", tmp_wav, BGM]
    elif shutil_which("ffmpeg"):
        cmd = ["ffmpeg", "-y", "-v", "error", "-i", BGM,
               "-ac", "1", "-ar", "44100", "-acodec", "pcm_s16le", tmp_wav]

    if cmd is None:
        print("WARNING: no mpg123 or ffmpeg found; skipping BGM decode.")
        return None

    r = subprocess.run(cmd, capture_output=True)
    if r.returncode != 0:
        print("WARNING: BGM decode failed: " + r.stderr.decode(errors="replace"))
        return None

    w = wave.open(tmp_wav, "rb")
    if w.getsampwidth() != 2 or w.getnchannels() != 1:
        err("decoded BGM is not 16-bit mono")
    ns = w.getnframes()
    samples = struct.unpack("<%dh" % ns, w.readframes(ns))
    w.close()
    os.remove(tmp_wav)

    # pad to multiple of 2048 samples for chunked streaming
    pad = (-ns) % 2048
    samples = list(samples) + [0] * pad
    total = len(samples)
    with open(raw_path, "wb") as f:
        f.write(struct.pack("<%dh" % total, *samples))
    print("wrote %s (%d samples = %.2fs, %d bytes)" % (
        raw_path, total, total / 44100.0, total * 2))
    return total


def shutil_which(name):
    import shutil
    return shutil.which(name)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def convert_icon():
    """Generate PSP EBOOT ICON0.PNG (144x80) and PIC1.PNG (480x272)."""
    from PIL import Image

    icon_src = os.path.join(SRC_DIR, "images", "IDI_PIKAICON-1_gap_filled_512.png")
    shot_src = os.path.join(SRC_DIR, "images", "screenshot.png")

    # ICON0: 144x80, pikachu icon scaled to fit, centered, transparent bg.
    icon0 = Image.new("RGBA", (144, 80), (0, 0, 0, 0))
    if os.path.exists(icon_src):
        im = Image.open(icon_src).convert("RGBA")
        scale = min(144.0 / im.width, 80.0 / im.height) * 0.95
        w, h = int(im.width * scale), int(im.height * scale)
        im = im.resize((w, h), Image.LANCZOS)
        icon0.paste(im, ((144 - w) // 2, (80 - h) // 2), im)
    icon0.save(os.path.join(OUT_ASSETS, "ICON0.PNG"))
    print("wrote %s" % os.path.join(OUT_ASSETS, "ICON0.PNG"))

    # PIC1: 480x272 background from the game screenshot.
    pic1 = Image.new("RGB", (480, 272), (0, 0, 0))
    if os.path.exists(shot_src):
        im = Image.open(shot_src).convert("RGB")
        scale = max(480.0 / im.width, 272.0 / im.height)
        w, h = int(im.width * scale), int(im.height * scale)
        im = im.resize((w, h), Image.LANCZOS)
        left = (w - 480) // 2
        top = (h - 272) // 2
        pic1.paste(im.crop((left, top, left + 480, top + 272)))
    pic1.save(os.path.join(OUT_ASSETS, "PIC1.PNG"))
    print("wrote %s" % os.path.join(OUT_ASSETS, "PIC1.PNG"))


def main():
    os.makedirs(OUT_ASSETS, exist_ok=True)
    os.makedirs(OUT_SRC, exist_ok=True)

    print("== converting atlas ==")
    convert_atlas()

    print("== converting sfx ==")
    convert_sfx()

    print("== converting bgm ==")
    bgm_total = decode_bgm()

    print("== converting icons ==")
    convert_icon()

    # Patch BGM_TOTAL_SAMPLES into audio_layout.h
    if bgm_total is not None:
        layout = os.path.join(OUT_SRC, "audio_layout.h")
        with open(layout) as f:
            txt = f.read()
        txt = txt.replace("#define BGM_TOTAL_SAMPLES 0",
                          "#define BGM_TOTAL_SAMPLES %d" % bgm_total)
        with open(layout, "w") as f:
            f.write(txt)
    print("== done ==")


if __name__ == "__main__":
    main()
