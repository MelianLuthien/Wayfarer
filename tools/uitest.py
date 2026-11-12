"""Render the menu on the host, to check how it looks without the console.

usage: python tools/uitest.py [scale] [outdir]      (default scale 2, outdir build/ui)

Compiles src/ui.h with a small host program, using the real state tables from src/wayfarer.c and the
art in build/art (run tools/build.py or the generators once first), and writes one PNG per
scenario: each tab, and the warning dialog. Needs zig (as the build does) and Pillow.
"""
import os
import re
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCALE = int(sys.argv[1]) if len(sys.argv) > 1 else 2
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "ui")
os.makedirs(OUT, exist_ok=True)

src = open(os.path.join(ROOT, "src", "wayfarer.c"), encoding="utf-8").read()
a = src.index('#include "destinations.h"')
state = src[a:src.index("/* ---- the controller hook")]
version = re.search(r'#define WF_VERSION "([^"]*)"', src).group(1)

main = r'''
#include <stdio.h>
#include <stdlib.h>
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;
#include "font.h"
#include "panel.h"
#include "logo.h"
#include "icons.h"
static const u8 *wf_font_blob, *wf_panel_blob, *wf_icon_blob, *wf_logo_blob;
#define WF_VERSION "%(version)s"
static void dbg(const char *s) { (void)s; }
static void dbgx(const char *s, u64 v) { (void)s; (void)v; }
static int world_running(void) { return 1; }
%(state)s
#include "ui.h"

static u8 *slurp(const char *dir, const char *name)
{
    char p[1024];
    snprintf(p, sizeof p, "%%s/%%s", dir, name);
    FILE *f = fopen(p, "rb");
    if (!f) { fprintf(stderr, "missing %%s\n", p); exit(2); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *b = malloc(n);
    if (fread(b, 1, n, f) != (size_t)n) exit(2);
    fclose(f);
    return b;
}

static void shot(const char *dir, const char *name)
{
    char p[1024];
    snprintf(p, sizeof p, "%%s/%%s.rgba", dir, name);
    FILE *f = fopen(p, "wb");
    fwrite(g_px, 1, (size_t)g_pitch * g_hpx, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *art = argv[1], *out = argv[2];
    g_S = atoi(argv[3]);
    wf_font_blob = slurp(art, "fonts.bin");
    wf_panel_blob = slurp(art, "panel.bin");
    wf_icon_blob = slurp(art, "icons64.bin");
    wf_logo_blob = slurp(art, "logo.bin");
    g_wpx = PW * g_S;
    g_hpx = PH * g_S;
    g_pitch = g_wpx * 4;
    g_px = calloc((size_t)g_pitch, g_hpx);
    g_ri = 0;
    g_bi = 2;
    g_cheat[0] = 1; g_cheat[1] = 1; g_cheat[3] = 1; g_cheat[5] = 1;
    g_speed = 2; g_jump = 1;
    for (int i = 0; i < ITEM_COUNT; i++) g_icount[i] = (i * 7) %% 5 == 0 ? 0 : (ITEM_IS_QTY(i) ? 10 + i : 1);

    g_tab = 0; g_sel = 0; paint_panel(); shot(out, "tab0");
    g_sel = 6; paint_panel(); shot(out, "tab0b");
    g_tab = 1; g_isel = 14; paint_panel(); shot(out, "tab1");
    g_isel = 62; paint_panel(); shot(out, "tab1b");
    g_tab = 2; g_wsel = 2; paint_panel(); shot(out, "tab2");
    g_wsel = 3; paint_panel(); shot(out, "tab2b");
    g_tab = 3; g_tfav = (1u << 4) | (1u << 20); g_tsort = 1; g_tl_ok = 0; g_tsel = 2; paint_panel(); shot(out, "tab3");
    g_tab = 0; g_dlg = 1; paint_panel(); shot(out, "dialog");
    return 0;
}
''' % {"version": version, "state": state}

work = os.path.join(OUT, "work")
os.makedirs(work, exist_ok=True)
open(os.path.join(work, "uitest_main.c"), "w", encoding="utf-8", newline="\n").write(main)
art = os.path.join(ROOT, "build", "art")
exe = os.path.join(work, "uitest.exe" if os.name == "nt" else "uitest")
subprocess.run([os.environ.get("ZIG", "zig"), "cc", "-O1", "-Wall", "-Wno-unused-function", "-I", os.path.join(ROOT, "src"), "-I", art,
                "-o", exe, os.path.join(work, "uitest_main.c")], check=True)
subprocess.run([exe, art, OUT, str(SCALE)], check=True)
w, h = 540 * SCALE, 340 * SCALE
for n in ("tab0", "tab0b", "tab1", "tab1b", "tab2", "tab2b", "tab3", "dialog"):
    d = open(os.path.join(OUT, n + ".rgba"), "rb").read()
    Image.frombytes("RGBA", (w, h), d).convert("RGB").save(os.path.join(OUT, n + ".png"))
    os.remove(os.path.join(OUT, n + ".rgba"))
print("wrote", OUT)
