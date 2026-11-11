"""Cut the AI-made art sheets into separate transparent PNGs.

usage: python tools/cutart.py [art_dir] [out_dir]
  art_dir  folder with sheetNN_frame.jpg files (default: wayfarer-art)
  out_dir  where the PNGs go (default: build/art, which is git-ignored)

Each sheet has a flat magenta (#FF00FF) background. The script:
  1. turns magenta into transparency (soft edge, magenta removed from the edge colours),
  2. finds every item as a connected blob and puts it in its grid cell (items may overlap a cell edge),
  3. saves each item trimmed (full/NAME.png) and fitted into a 128x128 square (icon128/NAME.png),
  4. makes a clearly marked placeholder for every item that has no art yet,
  5. writes manifest.json and a contact sheet (contact.png) to check the result by eye.
Needs only Pillow and numpy.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "wayfarer-art")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "art")

MAGENTA = np.array([255.0, 0.0, 255.0])
T_FG = 70          # distance from magenta below which a pixel is background
A0, A1 = 45, 120   # soft alpha ramp: distance A0 -> transparent, A1 -> opaque
BLOCK = 4          # blob finding works on 4x4 blocks
ICON = 128         # normalised icon size
PAD = 6

# file number -> (columns, rows, names in reading order). A name of None leaves the cell empty.
SHEETS = {
    2: (3, 2, ["tab_player", "tab_items", "tab_world", "tab_teleport", "tag_child", "tag_adult"]),
    3: (4, 3, ["row_god", "row_rupees", "row_magic", "row_items", "row_fly", "row_age", "row_time",
               "row_camera", "row_mark", "row_return", "row_info"]),
    4: (4, 3, ["sword_kokiri", "sword_master", "sword_giant", "sword_biggoron", "shield_deku",
               "shield_hylian", "shield_mirror", "tunic_goron", "tunic_zora", "boots_iron", "boots_hover"]),
    5: (4, 3, ["stick", "nut", "bomb", "bombchu", "slingshot", "seeds", "bow", "arrow", "arrow_fire",
               "arrow_ice", "arrow_light", "dins_fire"]),
    6: (4, 3, ["farores_wind", "nayrus_love", "ocarina_fairy", "ocarina_time", "hookshot", "longshot",
               "boomerang", "lens", "bean", "hammer", "bottle"]),
    7: (4, 2, ["mask_keaton", "mask_skull", "mask_spooky", "mask_bunny", "mask_goron", "mask_zora",
               "mask_gerudo", "mask_truth"]),
    # the AI drew sheet 8 as 5 x 3 with 14 items
    8: (5, 3, ["bracelet", "gauntlets_silver", "gauntlets_gold", "scale_silver", "scale_gold",
               "bombbag_1", "bombbag_2", "bombbag_3", "quiver_1", "quiver_2",
               "quiver_3", "seedbag_1", "seedbag_2", "seedbag_3"]),
    9: (4, 3, ["stone_kokiri", "stone_goron", "stone_zora", "medal_forest", "medal_fire", "medal_water",
               "medal_shadow", "medal_spirit", "medal_light", "stone_agony", "card_gerudo", "skulltula"]),
    # the missing-five upgrades sheet (file sheet08b_upgrades2.jpg, see FILES) and the brand sheet
    12: (3, 2, ["wallet_adult", "wallet_giant", "magic_meter", "magic_extended", "double_defense", None]),
    11: (2, 1, ["brand_logo", "brand_emblem"]),
}
FILES = {12: "sheet08b_upgrades2.jpg"}
EXTRAS = ["row_speed", "row_jump"]   # wayfarer-art/<name>.png, made by hand
FRAME = ["frame_corner", "frame_edge", "header_cap", "header_mid"]  # sheet 1, 2 x 2, cut by quarter

# Everything the menu will need that has no art yet. Each gets a placeholder tile.
MISSING = ["song_zelda", "song_epona", "song_saria", "song_sun", "song_time", "song_storms",
           "song_minuet", "song_bolero", "song_serenade", "song_nocturne", "song_requiem", "song_prelude"]


def dilate(m, n=1):
    for _ in range(n):
        p = np.pad(m, 1)
        m = (p[1:-1, 1:-1] | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
             | p[:-2, :-2] | p[:-2, 2:] | p[2:, :-2] | p[2:, 2:])
    return m


def label_blocks(m):
    """8-connected component labels of a small boolean grid (plain flood fill)."""
    h, w = m.shape
    lab = np.zeros((h, w), np.int32)
    n = 0
    for y in range(h):
        for x in range(w):
            if m[y, x] and not lab[y, x]:
                n += 1
                lab[y, x] = n
                stack = [(y, x)]
                while stack:
                    cy, cx = stack.pop()
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not lab[ny, nx]:
                                lab[ny, nx] = n
                                stack.append((ny, nx))
    return lab, n


def cut_region(rgb, d, region):
    """RGBA image of the pixels where `region` is true: soft alpha, magenta removed from the edges."""
    a = np.clip((d - A0) / (A1 - A0), 0, 1)
    a = np.where(region, a, 0.0)
    safe = np.maximum(a, 1e-3)[..., None]
    col = (rgb - (1 - a)[..., None] * MAGENTA) / safe
    col = np.clip(col, 0, 255)
    # edge pixels keep the colour of the solid pixels next to them (removes the pink/purple halo JPEG leaves)
    filled = a > 0.97
    partial = (a > 0.04) & ~filled
    for _ in range(4):
        ssum = np.zeros_like(col)
        cnt = np.zeros(a.shape)
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if dy or dx:
                    f = np.roll(filled, (dy, dx), (0, 1))
                    ssum += np.roll(col, (dy, dx), (0, 1)) * f[..., None]
                    cnt += f
        new_px = partial & ~filled & (cnt > 0)
        col[new_px] = ssum[new_px] / cnt[new_px][:, None]
        filled |= new_px
    out = np.dstack([col, a * 255]).astype(np.uint8)
    ys, xs = np.nonzero(a > 0.04)
    if len(ys) == 0:
        return None
    return Image.fromarray(out[ys.min():ys.max() + 1, xs.min():xs.max() + 1], "RGBA")


def load(path):
    rgb = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    d = np.sqrt(((rgb - MAGENTA) ** 2).sum(2))
    return rgb, d


def cut_grid(path, cols, rows, names):
    rgb, d = load(path)
    h, w = d.shape
    fg = d >= T_FG
    bh, bw = h // BLOCK, w // BLOCK
    blocks = fg[:bh * BLOCK, :bw * BLOCK].reshape(bh, BLOCK, bw, BLOCK).mean(axis=(1, 3)) > 0.15
    lab, n = label_blocks(dilate(blocks))
    # Column cuts: nominal grid lines, nudged to the emptiest column nearby, so items that touch a neighbour
    # (mask spikes, strings) are split where they meet and not through the middle of an item.
    edges = []
    for r in range(rows):
        band = fg[r * h // rows:(r + 1) * h // rows].sum(0).astype(float)
        row_edges = []
        for c in range(1, cols):
            nom = c * w // cols
            span = w // cols // 4
            xs = np.arange(max(1, nom - span), min(w - 1, nom + span))
            row_edges.append(int(xs[np.argmin(band[xs] + 0.01 * np.abs(xs - nom))]))
        edges.append(row_edges)

    def cell_of(cy, cx):
        r = min(rows - 1, int(cy / h * rows))
        return r * cols + sum(1 for e in edges[r] if cx >= e)

    cells = {}  # cell -> boolean block mask
    for i in range(1, n + 1):
        ys, xs = np.nonzero((lab == i) & blocks)
        if len(ys) < 12:
            continue
        per_cell = {}
        for y, x in zip(ys, xs):
            per_cell.setdefault(cell_of((y + 0.5) * BLOCK, (x + 0.5) * BLOCK), []).append((y, x))
        # a blob that touches a neighbour (spikes, strings) is split by cell; stray bits under 12% stay with the main part
        main = max(per_cell, key=lambda c: len(per_cell[c]))
        for c, pts in per_cell.items():
            target = c if len(pts) >= 0.12 * len(ys) else main
            m = cells.setdefault(target, np.zeros(blocks.shape, bool))
            for y, x in pts:
                m[y, x] = True
    found = {}
    for cell, small in sorted(cells.items()):
        if cell >= len(names) or names[cell] is None:
            print("  note: blob in unnamed cell", cell, "of", os.path.basename(path))
            continue
        small = dilate(small)
        region = np.kron(small, np.ones((BLOCK, BLOCK), bool))
        region = np.pad(region, ((0, h - region.shape[0]), (0, w - region.shape[1])))
        region = dilate(region, 2)
        img = cut_region(rgb, d, region)
        if img is not None:
            found[names[cell]] = (img, cell)
    for i, nm in enumerate(names):
        if nm and nm not in found:
            print("  MISSING cell", i, nm, "in", os.path.basename(path))
    return found


def cut_frame(path):
    rgb, d = load(path)
    h, w = d.shape
    found = {}
    for i, nm in enumerate(FRAME):
        r, c = divmod(i, 2)
        y0, y1, x0, x1 = r * h // 2, (r + 1) * h // 2, c * w // 2, (c + 1) * w // 2
        # keep clear of the seam in the middle of the sheet, where the AI left a pale line
        y0 += 8 if r else 0
        y1 -= 8 if not r else 0
        x0 += 8 if c else 0
        x1 -= 8 if not c else 0
        region = np.zeros((h, w), bool)
        region[y0:y1, x0:x1] = True
        img = cut_region(rgb, d, region)
        if img is not None:
            found[nm] = (img, i)
    return found


def fit(img, size=ICON, pad=PAD):
    s = min((size - 2 * pad) / img.width, (size - 2 * pad) / img.height)
    s = min(s, 1.0) if min(img.size) < 40 else s
    r = img.resize((max(1, round(img.width * s)), max(1, round(img.height * s))), Image.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(r, ((size - r.width) // 2, (size - r.height) // 2), r)
    return canvas


def font(size):
    for f in ("C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/arialbd.ttf", "C:/Windows/Fonts/arial.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, size)
    return ImageFont.load_default()


def placeholder(name, size=ICON):
    """A plain parchment tile with a dashed border and the item's name: unmistakably a stand-in."""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    dr = ImageDraw.Draw(im)
    dr.rounded_rectangle([4, 4, size - 5, size - 5], 14, fill=(216, 196, 154, 255), outline=(42, 29, 18, 255), width=3)
    for x in range(14, size - 14, 12):
        dr.line([x, 12, x + 6, 12], fill=(138, 106, 31, 255), width=2)
        dr.line([x, size - 13, x + 6, size - 13], fill=(138, 106, 31, 255), width=2)
    for y in range(14, size - 14, 12):
        dr.line([12, y, 12, y + 6], fill=(138, 106, 31, 255), width=2)
        dr.line([size - 13, y, size - 13, y + 6], fill=(138, 106, 31, 255), width=2)
    words = name.split("_")
    f = font(15)
    lines = ["TODO"] + words
    y = (size - len(lines) * 18) // 2
    for i, t in enumerate(lines):
        col = (160, 30, 30, 255) if i == 0 else (42, 29, 18, 255)
        tw = dr.textlength(t, font=f)
        dr.text(((size - tw) / 2, y + i * 18), t, font=f, fill=col)
    return im


# The song icons come from one game screenshot of the ocarina song ring (wayfarer-art/songs_screenshot.webp),
# not from a generated sheet: the game's own round icons, cut out by their circle.
SONG_SHOT = "songs_screenshot.webp"
SONG_R = 60   # radius of the part kept; the highlight ring around the selected icon lies outside it
SONGS = [("song_zelda", 160, 235), ("song_time", 328, 162), ("song_sun", 497, 140), ("song_storms", 665, 130),
         ("song_epona", 833, 140), ("song_saria", 1000, 175),
         ("song_minuet", 245, 402), ("song_bolero", 413, 355), ("song_serenade", 581, 328),
         ("song_requiem", 749, 326), ("song_nocturne", 917, 350), ("song_prelude", 1085, 402)]


def cut_songs(path):
    src = Image.open(path).convert("RGB")
    out = {}
    k = 4   # supersampled circle mask for a smooth edge
    for name, cx, cy in SONGS:
        r = SONG_R
        tile = src.crop((cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2)).convert("RGBA")
        m = Image.new("L", (tile.width * k, tile.height * k), 0)
        ImageDraw.Draw(m).ellipse([2 * k, 2 * k, (2 * r + 2) * k - 1, (2 * r + 2) * k - 1], fill=255)
        tile.putalpha(m.resize(tile.size, Image.LANCZOS))
        out[name] = tile.resize((ICON, ICON), Image.LANCZOS)
    return out


def main():
    os.makedirs(os.path.join(OUT, "full"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "icon128"), exist_ok=True)
    manifest = []
    items = []  # (name, icon128, placeholder)

    def save(name, full, sheet, cell):
        full.save(os.path.join(OUT, "full", name + ".png"))
        ic = fit(full)
        ic.save(os.path.join(OUT, "icon128", name + ".png"))
        manifest.append({"name": name, "sheet": sheet, "cell": cell, "placeholder": False,
                         "full_size": list(full.size)})
        items.append((name, ic, False))

    p1 = os.path.join(ART, "sheet01_frame.jpg")
    if os.path.exists(p1):
        for nm, (img, cell) in cut_frame(p1).items():
            img.save(os.path.join(OUT, "full", nm + ".png"))  # frame pieces keep their own shape: no square fit
            manifest.append({"name": nm, "sheet": 1, "cell": cell, "placeholder": False, "full_size": list(img.size)})
            items.append((nm, fit(img), False))
    for num, (cols, rows, names) in SHEETS.items():
        p = os.path.join(ART, FILES.get(num, "sheet%02d_frame.jpg" % num))
        if not os.path.exists(p):
            print("sheet", num, "not found, skipped")
            continue
        print("sheet", num)
        for nm, (img, cell) in cut_grid(p, cols, rows, names).items():
            save(nm, img, num, cell)
    for nm in EXTRAS:   # finished icons that need no cutting, saved straight into the art folder
        pe = os.path.join(ART, nm + ".png")
        if os.path.exists(pe):
            save(nm, Image.open(pe).convert("RGBA"), None, None)
    shot = os.path.join(ART, SONG_SHOT)
    have = set()
    if os.path.exists(shot):
        for nm, ic in cut_songs(shot).items():
            ic.save(os.path.join(OUT, "full", nm + ".png"))
            ic.save(os.path.join(OUT, "icon128", nm + ".png"))
            manifest.append({"name": nm, "sheet": None, "cell": None, "placeholder": False, "full_size": list(ic.size)})
            items.append((nm, ic, False))
            have.add(nm)
    for nm in MISSING:
        if nm in have:
            continue
        ph = placeholder(nm)
        ph.save(os.path.join(OUT, "icon128", nm + ".png"))
        manifest.append({"name": nm, "sheet": None, "cell": None, "placeholder": True})
        items.append((nm, ph, True))
    with open(os.path.join(OUT, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)

    # contact sheet on a dark wood colour, so fringes and holes show up
    per = 8
    cw, ch = ICON + 16, ICON + 30
    rws = (len(items) + per - 1) // per
    sheet = Image.new("RGBA", (per * cw, rws * ch), (43, 30, 19, 255))
    dr = ImageDraw.Draw(sheet)
    f = font(11)
    for i, (nm, ic, ph) in enumerate(items):
        x, y = (i % per) * cw + 8, (i // per) * ch + 4
        sheet.paste(ic, (x, y), ic)
        dr.text((x, y + ICON + 2), nm, font=f, fill=(255, 120, 120, 255) if ph else (243, 234, 210, 255))
    sheet.convert("RGB").save(os.path.join(OUT, "contact.png"))
    print("wrote", len(manifest), "entries;", sum(m["placeholder"] for m in manifest), "placeholders ->", OUT)


if __name__ == "__main__":
    main()
