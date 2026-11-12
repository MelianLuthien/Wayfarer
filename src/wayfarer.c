/*
 * Wayfarer: stage 2. Loads as a Switch ExeFS module (subsdk8), finds the game's
 * `main`, hooks the controller and the graphics present call, and shows a basic
 * cheat menu on R3. The cheats themselves are not wired up yet (stage 3).
 *
 * The hook plumbing (IPS trampolines + bridge table in main's bss) follows the
 * layout reverse-engineered from the Hyrule Cheat Menu rc1; see docs/RESEARCH.md.
 * GPL-3.0.
 */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;

#include "font.h"
#include "panel.h"
#include "logo.h"
#include "icons.h"
#include "iconblob.h"
#include "blobs.h"

#define TITLE_ID        0x0400486024DEC000ull
#define MAIN_RX_SIZE    0x3c47000ull   /* size of main's executable region */
#define MAIN_MOD0_WORD  0x3c4705cu     /* word at main+4: offset of MOD0 */
#define BRIDGE_OFFSET   0x4E9F800ull   /* bridge table inside main's bss */
#define BRIDGE_BYTES    0x80ull
#define NVN_LOADER      0x129E290ull   /* main: fn(device, getProcAddress) */
#define NVN_TABLE       0x4c8dde8ull   /* main: pointers to the game's NVN function slots */

void *memset(void *d, int c, unsigned long n)
{
    u8 *p = d;
    while (n--) *p++ = (u8)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
    u8 *p = d;
    const u8 *q = s;
    while (n--) *p++ = *q++;
    return d;
}

/* ---- supervisor calls ------------------------------------------------- */

static void dbg(const char *s)
{
    u64 n = 0;
    while (s[n]) n++;
    register u64 x0 __asm__("x0") = (u64)s;
    register u64 x1 __asm__("x1") = n;
    __asm__ volatile("svc #0x27"
                     : "+r"(x0), "+r"(x1)
                     :
                     : "x2", "x3", "x4", "x5", "x6", "x7", "memory");
}

static void dbgx(const char *label, u64 v)
{
    char b[96];
    int n = 0;
    b[n++] = '[';
    for (const char *p = "wayfarer] "; *p; p++) b[n++] = *p;
    for (; *label && n < 70; label++) b[n++] = *label;
    b[n++] = ' ';
    b[n++] = '0';
    b[n++] = 'x';
    for (int i = 60; i >= 0; i -= 4) b[n++] = "0123456789abcdef"[(v >> i) & 15];
    b[n++] = '\n';
    b[n] = 0;
    dbg(b);
}

typedef struct {
    u64 base, size;
    u32 type, attr, perm, ipc, dev, pad;
} MemInfo;

static int query_memory(u64 addr, MemInfo *mi)
{
    register u64 x0 __asm__("x0") = (u64)mi;
    register u64 x1 __asm__("x1") = 0;
    register u64 x2 __asm__("x2") = addr;
    __asm__ volatile("svc #6"
                     : "+r"(x0), "+r"(x1)
                     : "r"(x2)
                     : "x3", "x4", "x5", "x6", "x7", "memory");
    return (int)x0;
}

static int get_program_id(u64 *out)
{
    register u64 x0 __asm__("x0") = 0;
    register u64 x1 __asm__("x1") = 0x12;            /* InfoType_ProgramId */
    register u64 x2 __asm__("x2") = 0xFFFF8001ull;   /* current process */
    register u64 x3 __asm__("x3") = 0;
    __asm__ volatile("svc #0x29"
                     : "+r"(x0), "+r"(x1)
                     : "r"(x2), "r"(x3)
                     : "x4", "x5", "x6", "x7", "memory");
    *out = x1;
    return (int)x0;
}

/* ---- time and "is the world running" ------------------------------------ */

/* Bumped with every build (vYYMMDD.N). */
#define WF_VERSION "v261111.19"

static u64 now_ms(void)
{
    u64 t, f;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(t));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    f /= 1000;
    return f ? t / f : 0;
}

/* Stamped every time Link's update runs. The menu only opens (and stays open) while this is fresh,
 * which rules out the title screen, loading screens and the game's own menus. */
static volatile u64 g_link_tick;
#define WORLD_FRESH_MS 450
#define WORLD_STALE_CLOSE_MS 2500

static int world_running(void)
{
    u64 t = g_link_tick;
    return t && now_ms() - t < WORLD_FRESH_MS;
}

/* ---- menu state ------------------------------------------------------- */

#include "destinations.h"

#define TABS 4
#define PLAYER_ROWS 8
/* No static pointer tables anywhere: the module has no relocations, so any
 * absolute address stored in data would be wrong at runtime. */
/* Strings live in one blob and are found by walking it: a chain of ifs can be
 * compiled into a table of pointers, which would need relocations. */
static const char labels_blob[] = "God Mode\0Infinite Rupees\0Infinite Items\0Infinite Magic\0No-clip flight\0Chicken gravity\0Movement speed\0Jump height";
static const char descs_blob[] =
    "Hearts stay full; max is never raised\0"
    "Wallet tops up to its current size\0"
    "Ammo refills to your current limits\0"
    "Magic refills to the current limit\0"
    "Left stick flies, ZR up, ZL down\0"
    "Link floats down slowly, like carrying a chicken\0"
    "Left / Right sets how fast Link moves\0"
    "Left / Right sets how high Link jumps";
static const char tabs_blob[] = "Player\0Items\0World\0Teleport";
static const char wlabels_blob[] = "Time of day\0Mark position\0Return to mark\0Switch age\0Free camera";
static const char wdescs_blob[] =
    "Left / Right picks the hour, A sets it\0"
    "Remember where Link stands\0"
    "Go back to the marked place\0"
    "Child or adult; the map reloads\0"
    "Sticks move and look, ZR/ZL up and down, A faster";
#include "items.h"
#define WORLD_ROWS 5

static const char *nth(const char *blob, int i)
{
    while (i-- > 0) while (*blob++) {}
    return blob;
}
#define row_label(i) nth(labels_blob, (i))
#define row_desc(i) nth(descs_blob, (i))

static volatile int g_gen;       /* bumped whenever what the menu shows changes */
static volatile int g_open;      /* menu visible */
static volatile int g_sel;       /* selected row */
static volatile int g_dlg;       /* warning dialog visible */
static volatile int g_warned;    /* warning accepted this session */
static volatile int g_pending;   /* row waiting for the warning */
static volatile int g_tab;       /* 0 Player, 1 Items, 2 World, 3 Teleport */
static volatile int g_tsel;      /* selected destination */
static volatile int g_wsel;      /* selected World row */
static volatile int g_hour = 12; /* hour chosen on the World tab */
static volatile int g_isel;      /* selected inventory item */
volatile int g_world_req;        /* 1 time, 2 mark, 3 return, 4 age: served on the game thread */
volatile int g_item_req;         /* (item * 256 + op) + 1: served on the game thread */
static volatile int g_icount[ITEM_COUNT];   /* what the pouch holds, refreshed while the Items tab is open */
extern volatile int g_tp_req;    /* set here, served on the game thread (cheats.h) */
static volatile int g_cheat[8];  /* God, Rupees, Items, Magic, Flight, Free camera, Chicken gravity */
static volatile int g_speed;     /* 0..3: 1x, 1.5x, 2x, 3x */
static volatile int g_jump;      /* 0..3: 1x, 2x, 3x, 5x */
static volatile int g_stk[4];    /* raw sticks: left x, y, right x, y (before the game sees them) */
static volatile u64 g_held;      /* raw buttons held */

enum { B_A = 0, B_B = 1, B_X = 2, B_Y = 3, B_R3 = 5, B_L = 6, B_R = 7, B_LEFT = 12, B_UP = 13, B_RIGHT = 14, B_DOWN = 15 };
#define BIT(n) (1ull << (n))

static void menu_input_inner(u64 e);
static void menu_input(u64 e)
{
    menu_input_inner(e);
    g_gen++;
}

/* what: 0..4 flip that cheat; 5 and 61 speed up, 60 slow down; 50 teleport to the selected destination;
 * 100 + n a World action (n: 0 time, 1 mark, 2 return, 3 age, 4 free camera);
 * 1000 + item * 256 + op an inventory edit. */
static void activate(int what)
{
    if (what == 50) {
        g_tp_req = g_tsel + 1;
        g_open = 0;
        dbgx("teleport chosen", (u64)g_tsel);
    } else if (what >= 1000) {
        g_item_req = what - 1000 + 1;
    } else if (what == 104) {
        g_cheat[5] = !g_cheat[5];
        if (g_cheat[5]) g_open = 0;   /* look around with the menu closed; R3 reopens it */
    } else if (what >= 100) {
        g_world_req = what - 100 + 1;
        if (what == 103) g_open = 0;   /* the map reloads */
    } else if (what == 5 || what == 61) {
        g_speed = (g_speed + 1) & 3;
    } else if (what == 60) {
        g_speed = (g_speed + 3) & 3;
    } else if (what == 62) {
        g_jump = (g_jump + 3) & 3;
    } else if (what == 63) {
        g_jump = (g_jump + 1) & 3;
    } else {
        g_cheat[what] = !g_cheat[what];
    }
}

/* The first game-changing action of a session shows the warning. */
static void request(int what)
{
    if (!g_warned) { g_pending = what; g_dlg = 1; }
    else activate(what);
}

static void menu_input_inner(u64 e)
{
    if (e & BIT(B_R3)) {
        if (!g_open && !world_running()) { dbg("[wayfarer] R3 ignored: the game is not running right now\n"); return; }
        g_open = !g_open;
        g_dlg = 0;
        if (g_open) dbg("[wayfarer] menu open\n"); else dbg("[wayfarer] menu closed\n");
        return;
    }
    if (!g_open) return;
    if (g_dlg) {
        if (e & BIT(B_A)) {
            g_warned = 1;
            g_dlg = 0;
            activate(g_pending);
        } else if (e & BIT(B_B)) {
            g_dlg = 0;
        }
        return;
    }
    if (e & BIT(B_L)) g_tab = (g_tab + TABS - 1) % TABS;
    if (e & BIT(B_R)) g_tab = (g_tab + 1) % TABS;
    if (e & BIT(B_B)) { g_open = 0; return; }
    if (g_tab == 0) {
        if (e & BIT(B_UP)) g_sel = (g_sel + PLAYER_ROWS - 1) % PLAYER_ROWS;
        if (e & BIT(B_DOWN)) g_sel = (g_sel + 1) % PLAYER_ROWS;
        if (g_sel == 6) {
            if (e & BIT(B_LEFT)) request(60);
            if (e & BIT(B_RIGHT)) request(61);
        }
        if (g_sel == 7) {
            if (e & BIT(B_LEFT)) request(62);
            if (e & BIT(B_RIGHT)) request(63);
        }
        if (e & BIT(B_A)) request(g_sel == 6 ? 61 : g_sel == 7 ? 63 : g_sel == 5 ? 6 : g_sel);
    } else if (g_tab == 1) {
        int s = g_isel;
        if (e & BIT(B_LEFT)) s = s > 0 ? s - 1 : ITEM_COUNT - 1;
        if (e & BIT(B_RIGHT)) s = s < ITEM_COUNT - 1 ? s + 1 : 0;
        if ((e & BIT(B_UP)) && s >= 12) s -= 12;
        if ((e & BIT(B_DOWN)) && s + 12 < ITEM_COUNT) s += 12;
        g_isel = s;
        if (e & BIT(B_A)) request(1000 + s * 256 + 200);
        if (ITEM_IS_QTY(s)) {
            if (e & BIT(B_X)) request(1000 + s * 256 + 65);
            if (e & BIT(B_Y)) request(1000 + s * 256 + 63);
        }
    } else if (g_tab == 2) {
        if (e & BIT(B_UP)) g_wsel = (g_wsel + WORLD_ROWS - 1) % WORLD_ROWS;
        if (e & BIT(B_DOWN)) g_wsel = (g_wsel + 1) % WORLD_ROWS;
        if (g_wsel == 0) {
            if (e & BIT(B_LEFT)) g_hour = (g_hour + 23) % 24;
            if (e & BIT(B_RIGHT)) g_hour = (g_hour + 1) % 24;
        }
        if (e & BIT(B_A)) request(100 + g_wsel);
    } else if (g_tab == 3) {
        if (e & BIT(B_UP)) g_tsel = (g_tsel + TP_COUNT - 1) % TP_COUNT;
        if (e & BIT(B_DOWN)) g_tsel = (g_tsel + 1) % TP_COUNT;
        if (e & BIT(B_LEFT)) g_tsel = g_tsel >= 7 ? g_tsel - 7 : 0;
        if (e & BIT(B_RIGHT)) g_tsel = g_tsel + 7 < TP_COUNT ? g_tsel + 7 : TP_COUNT - 1;
        if (e & BIT(B_A)) request(50);
    }
}

/* ---- the controller hook ---------------------------------------------- */

static u64 g_base;          /* runtime address of main */
static u64 g_prev[5];       /* previous buttons per hooked input function */

/* Offsets (from main) of the five input functions the hooks call through to. */
const u64 wf_npad_fn[5] = {0x3c427f0, 0x3c42810, 0x3c42820, 0x3c42800, 0x3c42830};

/*
 * Replaces a call to one of the game's pad-state readers
 *   long fn(State *out, int count, const u32 *npad_id)
 * Calls the original, feeds button edges to the menu, and hides all input from
 * the game while the menu is open. State layout: u64 sample, u64 buttons,
 * s32 stickL[2], s32 stickR[2], u32 attributes, stride 0x28.
 */
long wf_npad(void *out, int count, const u32 *id, u64 fn_off, u64 idx)
{
    long (*orig)(void *, int, const u32 *) = (long (*)(void *, int, const u32 *))(g_base + fn_off);
    long n = orig(out, count, id);
    if (n >= 1 && id && (*id == 0 || *id == 0x20)) {
        u8 *o = (u8 *)out;
        int was_open = g_open;
        if (*(u32 *)(o + 0x20) & 1) {   /* connected */
            u64 b = *(u64 *)(o + 8);
            u64 e = b & ~g_prev[idx];
            g_prev[idx] = b;
            g_stk[0] = *(int *)(o + 0x10); g_stk[1] = *(int *)(o + 0x14);
            g_stk[2] = *(int *)(o + 0x18); g_stk[3] = *(int *)(o + 0x1c);
            g_held = b;
            if (e) menu_input(e);
        } else {
            g_prev[idx] = 0;
        }
        if (g_open && now_ms() - g_link_tick > WORLD_STALE_CLOSE_MS) {
            g_open = 0;
            g_dlg = 0;
            g_gen++;
            dbg("[wayfarer] menu closed: the game stopped running\n");
        }
        if (was_open || g_open) {
            g_stk[0] = g_stk[1] = g_stk[2] = g_stk[3] = 0;   /* the menu is not a joystick for the cheats */
            g_held = 0;
        }
        if (was_open || g_open || g_cheat[5]) {   /* menu or free camera: the game sees no input */
            for (long i = 0; i < n; i++) {
                u8 *s = o + i * 0x28;
                *(u64 *)(s + 8) = 0;
                *(u64 *)(s + 0x10) = 0;
                *(u64 *)(s + 0x18) = 0;
            }
        } else if (g_cheat[4]) {   /* flight: the stick and ZL/ZR steer the flight, not Link */
            for (long i = 0; i < n; i++) {
                u8 *s = o + i * 0x28;
                *(u64 *)(s + 8) &= ~(BIT(8) | BIT(9));
                *(u64 *)(s + 0x10) = 0;
            }
        }
    }
    return n;
}

#define THUNK(i, off)                              \
    ".global wf_thunk" #i "\nwf_thunk" #i ":\n"    \
    "  adrp x5, wf_npad_fn\n"                      \
    "  add  x5, x5, :lo12:wf_npad_fn\n"            \
    "  ldr  x3, [x5, #" #off "]\n"                 \
    "  mov  x4, #" #i "\n"                         \
    "  b    wf_npad\n"

__asm__(".section .text.thunks,\"ax\"\n" THUNK(0, 0) THUNK(1, 8) THUNK(2, 16) THUNK(3, 24) THUNK(4, 32) ".text\n");

extern char wf_thunk0[], wf_thunk1[], wf_thunk2[], wf_thunk3[], wf_thunk4[];

#include "cheats.h"

/* ---- NVN (graphics API) ----------------------------------------------- */

typedef void *(*GetProc)(void *, const char *);

static void *g_dev;
static void (*n_present)(void *, void *, int);
static void (*n_setTex)(void *, int, void *const *);
static int (*n_winInit)(void *, void *);
static void (*n_winFini)(void *);
static void (*n_getCrop)(void *, int *);
static int (*n_texW)(void *);
static int (*n_texH)(void *);
static int (*n_texFmt)(void *);
static int (*n_texTarget)(void *);
static int (*n_texSamples)(void *);
static void (*n_mpbDefaults)(void *);
static void (*n_mpbDev)(void *, void *);
static void (*n_mpbStorage)(void *, void *, u64);
static void (*n_mpbFlags)(void *, int);
static int (*n_mpInit)(void *, void *);
static u64 (*n_mpAddr)(void *);
static void *(*n_mpMap)(void *);
static void (*n_mpFlush)(void *, u64, u64);
static void (*n_mpInval)(void *, u64, u64);
static int (*n_syncInit)(void *, void *);
static int (*n_syncWait)(void *, u64);
static int (*n_cbInit)(void *, void *);
static void (*n_cbAddCmd)(void *, void *, u64, u64);
static void (*n_cbAddCtl)(void *, void *, u64);
static void (*n_cbBegin)(void *);
static u64 (*n_cbEnd)(void *);
static void (*n_qSubmit)(void *, int, u64 *);
static void (*n_qFlush)(void *);
static void (*n_cbFence)(void *, void *, int, int);
static void (*n_cbStride)(void *, long);
static void (*n_cbT2B)(void *, void *, void *, const int *, u64, int);
static void (*n_cbB2T)(void *, u64, void *, void *, const int *, int);
static void (*n_cbBarrier)(void *, int);

#define R(name, var)                                                          \
    do {                                                                       \
        *(void **)&var = gp(dev, name);                                        \
        if (!var) { dbg("[wayfarer] missing NVN function " name "\n"); return 0; } \
    } while (0)

static int resolve_nvn(void *dev, GetProc gp)
{
    R("nvnQueuePresentTexture", n_present);
    R("nvnWindowBuilderSetTextures", n_setTex);
    R("nvnWindowInitialize", n_winInit);
    R("nvnWindowFinalize", n_winFini);
    R("nvnWindowGetCrop", n_getCrop);
    R("nvnTextureGetWidth", n_texW);
    R("nvnTextureGetHeight", n_texH);
    R("nvnTextureGetFormat", n_texFmt);
    R("nvnTextureGetTarget", n_texTarget);
    R("nvnTextureGetSamples", n_texSamples);
    R("nvnMemoryPoolBuilderSetDefaults", n_mpbDefaults);
    R("nvnMemoryPoolBuilderSetDevice", n_mpbDev);
    R("nvnMemoryPoolBuilderSetStorage", n_mpbStorage);
    R("nvnMemoryPoolBuilderSetFlags", n_mpbFlags);
    R("nvnMemoryPoolInitialize", n_mpInit);
    R("nvnMemoryPoolGetBufferAddress", n_mpAddr);
    R("nvnMemoryPoolMap", n_mpMap);
    R("nvnMemoryPoolFlushMappedRange", n_mpFlush);
    R("nvnMemoryPoolInvalidateMappedRange", n_mpInval);
    R("nvnSyncInitialize", n_syncInit);
    R("nvnSyncWait", n_syncWait);
    R("nvnCommandBufferInitialize", n_cbInit);
    R("nvnCommandBufferAddCommandMemory", n_cbAddCmd);
    R("nvnCommandBufferAddControlMemory", n_cbAddCtl);
    R("nvnCommandBufferBeginRecording", n_cbBegin);
    R("nvnCommandBufferEndRecording", n_cbEnd);
    R("nvnQueueSubmitCommands", n_qSubmit);
    R("nvnQueueFlush", n_qFlush);
    R("nvnCommandBufferFenceSync", n_cbFence);
    R("nvnCommandBufferSetCopyRowStride", n_cbStride);
    R("nvnCommandBufferCopyTextureToBuffer", n_cbT2B);
    R("nvnCommandBufferCopyBufferToTexture", n_cbB2T);
    R("nvnCommandBufferBarrier", n_cbBarrier);
    return 1;
}

/* Swapchain bookkeeping, filled by the wrapped window calls. */
#define MAXTEX 8
static void *g_win;
static void *g_tex[MAXTEX];
static int g_ntex;
static void *g_bld;
static void *g_bldtex[MAXTEX];
static int g_bldn;

static void wf_setTex(void *builder, int count, void *const *tex)
{
    n_setTex(builder, count, tex);
    g_bld = builder;
    g_bldn = count > MAXTEX ? MAXTEX : count;
    for (int i = 0; i < g_bldn; i++) g_bldtex[i] = tex[i];
}

static int wf_winInit(void *win, void *builder)
{
    int r = n_winInit(win, builder);
    if (builder == g_bld) {
        g_win = win;
        g_ntex = g_bldn;
        for (int i = 0; i < g_ntex; i++) g_tex[i] = g_bldtex[i];
    }
    return r;
}

static void wf_winFini(void *win)
{
    if (win == g_win) { g_win = 0; g_ntex = 0; }
    n_winFini(win);
}

/* GPU-visible scratch: command memory in the first 0x10000, then the panel pixels. */
#define POOL_BYTES 0x980000
#define BUF_OFF    0x10000
static u8 g_poolmem[POOL_BYTES] __attribute__((aligned(4096)));
static u8 g_pool[0x400] __attribute__((aligned(64)));
static u8 g_mpb[0x100] __attribute__((aligned(64)));
static u8 g_sync[0x100] __attribute__((aligned(64)));
static u8 g_cb[2][0x400] __attribute__((aligned(64)));
static u8 g_ctl[2][0x1000] __attribute__((aligned(16)));
static int g_ready, g_fail, g_logged;
#if WF_READBACK
static int g_timeouts;
#endif
static u8 *g_cpu;
static u64 g_gpu;

static int nvn_prepare(void)
{
    if (g_ready) return 1;
    n_mpbDefaults(g_mpb);
    n_mpbDev(g_mpb, g_dev);
    n_mpbStorage(g_mpb, g_poolmem, POOL_BYTES);
    n_mpbFlags(g_mpb, 0x24);               /* CPU cached | GPU cached */
    if (!n_mpInit(g_pool, g_mpb)) { dbg("[wayfarer] pool init failed\n"); return 0; }
    g_cpu = (u8 *)n_mpMap(g_pool);
    if (!g_cpu) g_cpu = g_poolmem;
    g_gpu = n_mpAddr(g_pool);
    if (!n_syncInit(g_sync, g_dev)) { dbg("[wayfarer] sync init failed\n"); return 0; }
    for (int i = 0; i < 2; i++)
        if (!n_cbInit(g_cb[i], g_dev)) { dbg("[wayfarer] cmdbuf init failed\n"); return 0; }
    g_ready = 1;
    dbg("[wayfarer] overlay ready\n");
    return 1;
}

static void rec_begin(int i)
{
    n_cbAddCmd(g_cb[i], g_pool, (u64)i * 0x1000, 0x1000);
    n_cbAddCtl(g_cb[i], g_ctl[i], 0x1000);
    n_cbBegin(g_cb[i]);
}

static void rec_submit(void *queue, int i)
{
    u64 h = n_cbEnd(g_cb[i]);
    n_qSubmit(queue, 1, &h);
}

/* ---- software drawing into the panel buffer ---------------------------- */

/* 1 = read the game pixels back and blend (translucent panel); needs the GPU fence to
 * work in the emulator, which it did not in the first try, so 0 = opaque panel. */
#ifndef WF_READBACK
#define WF_READBACK 0
#endif
#include "ui.h"

static void draw_menu(void *queue, void *tex, void *win)
{
    if (g_fail) return;
    int fmt = n_texFmt(tex), tw = n_texW(tex), th = n_texH(tex);
    if (!g_logged) {
        g_logged = 1;
        dbgx("swapchain format", (u64)fmt);
        dbgx("swapchain width", (u64)tw);
        dbgx("swapchain height", (u64)th);
    }
    if (n_texTarget(tex) != 1 || n_texSamples(tex) > 1 || !(fmt == 37 || fmt == 55 || fmt == 56)) {
        dbg("[wayfarer] swapchain format unsupported; no overlay\n");
        g_fail = 1;
        return;
    }
    g_ri = fmt == 55 ? 2 : 0;
    g_bi = fmt == 55 ? 0 : 2;
    if (!nvn_prepare()) { g_fail = 1; return; }

    int crop[4] = {0, 0, tw, th};
    n_getCrop(win, crop);
    if (crop[2] <= 0 || crop[3] <= 0 || crop[0] < 0 || crop[1] < 0 || crop[0] + crop[2] > tw || crop[1] + crop[3] > th) {
        crop[0] = 0; crop[1] = 0; crop[2] = tw; crop[3] = th;
    }

    int S = (crop[3] + 135) / 540;
    if (S < 1) S = 1;
    for (;; S--) {
        int w = PW * S, h = PH * S;
        long stride = ((long)w * 4 + 0x3f) & ~0x3fL;
        if (w <= crop[2] && h <= crop[3] && stride * h <= POOL_BYTES - BUF_OFF) break;
        if (S <= 1) return;
    }
    int w = PW * S, h = PH * S;
    long stride = ((long)w * 4 + 0x3f) & ~0x3fL;
    int region[6] = {crop[0] + (crop[2] - w) / 2, crop[1] + (crop[3] - h) / 2, 0, w, h, 1};
    u64 buf = g_gpu + BUF_OFF;

#if WF_READBACK
    /* 1: copy the game's pixels under the panel into the buffer, wait for it. */
    rec_begin(0);
    n_cbBarrier(g_cb[0], 3);
    n_cbStride(g_cb[0], stride);
    n_cbT2B(g_cb[0], tex, 0, region, buf, 0);
    n_cbFence(g_cb[0], g_sync, 0, 0);
    rec_submit(queue, 0);
    n_qFlush(queue);   /* the emulator only runs queued work after a flush */
    if (n_syncWait(g_sync, 20000000ull) >= 2) {
        dbg("[wayfarer] copy wait failed\n");
        if (++g_timeouts >= 3) { g_fail = 1; dbg("[wayfarer] overlay disabled after repeated timeouts\n"); }
        return;
    }
    n_mpInval(g_pool, BUF_OFF, (u64)stride * h);

#endif
    /* 2: paint the panel into the buffer, but only when it changed: the GPU reads
     * this buffer every frame and nothing synchronises with it. */
    static int painted = -1;
    if (painted != g_gen) {
        painted = g_gen;
        g_px = g_cpu + BUF_OFF;
        g_pitch = (int)stride;
        g_S = S;
        g_wpx = w;
        g_hpx = h;
        paint_panel();
        n_mpFlush(g_pool, BUF_OFF, (u64)stride * h);
    }

    /* 3: copy it onto the screen. */
    rec_begin(1);
    n_cbStride(g_cb[1], stride);
    n_cbB2T(g_cb[1], buf, tex, 0, region, 0);
    n_cbBarrier(g_cb[1], 3);
    rec_submit(queue, 1);
}

static void wf_present(void *queue, void *win, int idx)
{
    if (g_open && win == g_win && idx >= 0 && idx < g_ntex) draw_menu(queue, g_tex[idx], win);
    n_present(queue, win, idx);
}

/*
 * Replaces the call to the game's NVN loader fn(device, getProcAddress): run it,
 * then (once) look up the functions we need and swap our wrappers into the
 * game's function-pointer slots.
 */
u64 wf_nvn_load(void *dev, GetProc gp)
{
    u64 r = ((u64 (*)(void *, void *))(g_base + NVN_LOADER))(dev, (void *)gp);
    static int done;
    if (!dev || !gp || done) return r;
    done = 1;

    if (!resolve_nvn(dev, gp)) return r;
    g_dev = dev;

    u64 offs[4];
    offs[0] = 0x0; offs[1] = 0x20; offs[2] = 0xa8; offs[3] = 0xb0;
    void *orig[4], *wrap[4];
    orig[0] = (void *)n_present; orig[1] = (void *)n_setTex; orig[2] = (void *)n_winInit; orig[3] = (void *)n_winFini;
    wrap[0] = (void *)wf_present; wrap[1] = (void *)wf_setTex; wrap[2] = (void *)wf_winInit; wrap[3] = (void *)wf_winFini;
    u64 *slot[4];
    for (int i = 0; i < 4; i++) {
        MemInfo mi;
        slot[i] = (u64 *)*(u64 *)(g_base + NVN_TABLE + offs[i]);
        if (!slot[i] || query_memory((u64)slot[i], &mi) != 0 || (mi.perm & 3) != 3 || *slot[i] != (u64)orig[i]) {
            dbgx("NVN slot mismatch, index", (u64)i);
            return r;
        }
    }
    for (int i = 0; i < 4; i++) *slot[i] = (u64)wrap[i];
    __asm__ volatile("dmb ish" ::: "memory");
    dbg("[wayfarer] NVN wrappers installed\n");
    return r;
}

/* ---- entry ------------------------------------------------------------ */

/* Bisecting builds: 0 = empty init, 1 = log one line only, 2 = full (default). */
#ifndef WF_VARIANT
#define WF_VARIANT 2
#endif
#if WF_VARIANT == 0
#define WF_INIT_SYM "_start"
#else
#define WF_INIT_SYM "wf_init"
#endif

void wf_init(void)
{
    if (WF_VARIANT == 1) {
        dbg("[wayfarer] variant B alive\n");
        return;
    }
    u64 pid = 0;
    if (get_program_id(&pid) != 0 || pid != TITLE_ID) {
        dbg("[wayfarer] not the expected title; idle\n");
        return;
    }

    MemInfo mi;
    u64 addr = (u64)(void *)&wf_init;
    u64 base = 0;
    for (int i = 0; i < 64; i++) {
        if (query_memory(addr, &mi) != 0) break;
        if (mi.perm == 5 && mi.size == MAIN_RX_SIZE && *(const u32 *)(mi.base + 4) == MAIN_MOD0_WORD) {
            base = mi.base;
            break;
        }
        if (mi.base == 0) break;
        addr = mi.base - 1;
    }
    if (!base) {
        dbg("[wayfarer] main not found; idle\n");
        return;
    }
    g_base = base;

    u64 slots = base + BRIDGE_OFFSET;
    if (query_memory(slots, &mi) != 0 || (mi.perm & 3) != 3 || mi.base + mi.size < slots + BRIDGE_BYTES) {
        dbg("[wayfarer] bridge table not writable; idle\n");
        return;
    }
    volatile u64 *t = (volatile u64 *)slots;
    for (int i = 0; i < (int)(BRIDGE_BYTES / 8); i++) {
        if (t[i]) {
            dbg("[wayfarer] bridge table occupied (another mod?); idle\n");
            return;
        }
    }

    t[0] = (u64)wf_thunk0;   /* 0x800 */
    t[1] = (u64)wf_thunk1;   /* 0x808 */
    t[2] = (u64)wf_thunk2;   /* 0x810 */
    t[3] = (u64)wf_thunk3;   /* 0x818 */
    t[4] = (u64)wf_thunk4;   /* 0x820 */
    t[5] = (u64)wf_nvn_load; /* 0x828 */
    wf_actor_orig = base + 0x3c4631c;
    t[9] = (u64)wf_actor_thunk; /* 0x848 */
    t[15] = (u64)wf_cam_hook;   /* 0x878 */
    __asm__ volatile("dmb ish" ::: "memory");
    /* size test for the embedded art: if the module loaded, these lines show the blob is mapped and readable */
    dbgx("icons bytes", (u64)(wf_icon_blob_end - wf_icon_blob));
    dbgx("icons count", ICON_COUNT);
    dbgx("icons checksum", icons_checksum());
    dbg("[wayfarer] " WF_VERSION " loaded; press R3 for the menu\n");
}

/* ---- module header: entry stub, MOD0, dynamic section ------------------ */

__asm__(
    ".section .text.entry,\"ax\"\n"
    ".global _start\n"
    "_start:\n"
    "  ret\n"
    "  .word mod0\n"                /* offset of MOD0 from the module base (base is 0) */

    ".section .data.mod0,\"aw\"\n"
    ".balign 8\n"
    "mod0:\n"
    "  .ascii \"MOD0\"\n"
    "  .word dynamic - mod0\n"
    "  .word __bss_start - . + 8\n"   /* bss start, relative to MOD0 */
    "  .word __bss_end - . + 8\n"
    "  .word 0\n"                      /* eh_frame_hdr start */
    "  .word 0\n"                      /* eh_frame_hdr end */
    "  .word __bss_start - . + 0x18\n" /* runtime module object: first bytes of bss, written by rtld */
    ".balign 8\n"
    "dynamic:\n"
    "  .quad 7,  wf_rela\n"            /* DT_RELA */
    "  .quad 8,  0\n"                  /* DT_RELASZ */
    "  .quad 9,  24\n"                 /* DT_RELAENT */
    "  .quad 0x6ffffff9, 0\n"          /* DT_RELACOUNT */
    "  .quad 6,  wf_symtab\n"          /* DT_SYMTAB */
    "  .quad 11, 24\n"                 /* DT_SYMENT */
    "  .quad 5,  wf_strtab\n"          /* DT_STRTAB */
    "  .quad 10, 4\n"                  /* DT_STRSZ */
    "  .quad 4,  wf_hash\n"            /* DT_HASH */
    "  .quad 12, " WF_INIT_SYM "\n"    /* DT_INIT */
    "  .quad 0,  0\n"                  /* DT_NULL */

    ".section .bss.modobj,\"aw\",@nobits\n"
    ".balign 16\n"
    "  .zero 0x400\n"
    ".section .rodata.dyn,\"a\"\n"
    ".balign 8\n"
    "wf_symtab:\n  .zero 24\n"
    "wf_strtab:\n  .zero 4\n"
    "wf_hash:\n  .word 1, 1, 0, 0\n"
    "wf_rela:\n"
    ".text\n");
