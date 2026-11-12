"""Build the live menu mockup: the real menu code (src/ui.h and the menu logic of src/wayfarer.c)
compiled to WebAssembly, so tools/mockserver.py can show it in a browser and be driven by keys.

usage: python tools/mockbuild.py [--zig PATH]     (output: build/mock/menu.wasm)
"""
import argparse
import os
import re
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser()
ap.add_argument("--zig", default=os.environ.get("ZIG", "zig"))
args = ap.parse_args()

src = open(os.path.join(ROOT, "src", "wayfarer.c"), encoding="utf-8").read().replace("\r\n", "\n")
a = src.index('#include "destinations.h"')
b = src.index("/* ---- the controller hook")
logic = src[a:b]
version = re.search(r'#define WF_VERSION "([^"]*)"', src).group(1)

main = r'''
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;
#include "font.h"
#include "panel.h"
#include "logo.h"
#include "icons.h"
static const u8 *wf_font_blob, *wf_panel_blob, *wf_icon_blob, *wf_logo_blob;
#define WF_VERSION "%(version)s"
#define dbg(s) ((void)0)
#define dbgx(a, b) ((void)0)
static int world_running(void) { return 1; }
volatile int g_tp_req;
%(logic)s
#include "ui.h"

static u8 heap[10 << 20];
static u8 pixels[1080 * 680 * 4];
static unsigned heap_top;

u8 *wf_alloc(int n)
{
    u8 *p = heap + heap_top;
    heap_top += (unsigned)(n + 15) & ~15u;
    return p;
}

void wf_setup(u8 *font, u8 *panel, u8 *icons, u8 *logo)
{
    wf_font_blob = font; wf_panel_blob = panel; wf_icon_blob = icons; wf_logo_blob = logo;
    g_S = 2; g_wpx = PW * 2; g_hpx = PH * 2; g_pitch = g_wpx * 4; g_px = pixels;
    g_ri = 0; g_bi = 2;
    g_open = 1;
    for (int i = 0; i < ITEM_COUNT; i++) g_icount[i] = (i * 7) %% 5 == 0 ? 0 : (ITEM_IS_QTY(i) ? 10 + i : 1);
}

/* the game serves item edits on its own thread; here they are approximated so the Items tab reacts */
static void serve_items(void)
{
    if (!g_item_req) return;
    int r = g_item_req - 1;
    g_item_req = 0;
    int it = r >> 8, op = r & 255;
    if (it < 0 || it >= ITEM_COUNT) return;
    if (op == 200) g_icount[it] = g_icount[it] ? 0 : (ITEM_IS_QTY(it) ? 10 : 1);
    else if (op == 65) g_icount[it] += 5;
    else if (op == 63) g_icount[it] = g_icount[it] > 5 ? g_icount[it] - 5 : 0;
}

void wf_key(int bit)
{
    menu_input_inner(1ull << bit);
    serve_items();
    g_world_req = 0;
    if (g_tp_req) { g_tp_req = 0; }
    g_open = 1;
}

u8 *wf_paint(void)
{
    paint_panel();
    return pixels;
}
''' % {"version": version, "logic": logic}

out = os.path.join(ROOT, "build", "mock")
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "mock.c"), "w", encoding="utf-8", newline="\n").write(main)
art = os.path.join(ROOT, "build", "art")
subprocess.run([args.zig, "cc", "-target", "wasm32-freestanding", "-O2", "-nostdlib", "-Wno-unused-function", "-Wno-unused-variable",
                "-Wl,--no-entry", "-Wl,--export=wf_alloc", "-Wl,--export=wf_setup", "-Wl,--export=wf_key", "-Wl,--export=wf_paint",
                "-Wl,--initial-memory=16777216", "-I", os.path.join(ROOT, "src"), "-I", art,
                "-o", os.path.join(out, "menu.wasm"), os.path.join(out, "mock.c")], check=True)
print("built", os.path.join(out, "menu.wasm"), version)
