/* The cheats. Included by wayfarer.c after the menu state and the bridge constants. */

/*
 * All offsets are from main. The logic follows what the Hyrule Cheat Menu rc1 does
 * (found by reading its code), simplified: everything runs inside the per-frame update
 * of the player actor, so it only ever happens while Link exists.
 * "Infinite" always means: top up to the current limit, never above it.
 */
#define OFF_GD_HOLDER   0x4c7e180ull   /* -> holder -> game data manager */
#define OFF_EV_HOLDER   0x4c7e090ull   /* -> holder -> event manager */
#define OFF_STATE       0x4c7e2a8ull
#define OFF_PM          0x4e78e28ull   /* -> pouch manager */
#define OFF_ACTORDB     0x4e96888ull
#define FN_GET          0x17b3b0ull    /* (gd, int *out, ctx, hash) -> ok */
#define FN_SET          0x9b39e0ull    /* (gd, value, ctx, hash) */
#define FN_BOOLGET      0x20fd90ull    /* (gd, u8 *out, hash) -> ok */
#define FN_EVENT        0xbcc100ull    /* (event manager, 0) -> nonzero while an event plays */
#define FN_POUCH_COUNT  0x5963e0ull    /* (pm, &name, 0, 0) -> count, negative if unknown */
#define FN_POUCH_MAX    0x595cf0ull    /* (pm, &name, 0, 0) -> capacity */
#define FN_POUCH_ADD    0x110d570ull   /* (pm, &name, 0, delta, 0, 0, 0, 0, 0) */
#define FN_MAGIC_MAX    0xa676a0ull    /* (pm) */
#define FN_MAGIC_CUR    0xa67c30ull    /* (pm, 0) */
#define FN_AGE          0x4b6db0ull    /* () -> nonzero while Link is a child */
#define FN_MAGIC_COMP   0x944e50ull    /* (link actor) -> component that holds the shown magic */
#define FN_MAGIC_FILL   0x2e80f80ull   /* (component) */
#define MAGIC_COMP_VT   0x4b4dfc0ull   /* vtable of that component */
#define POUCH_CTX       0x3bc0ull      /* added to the pouch manager to get the data context */
#define H_MAGIC_MAX     0x39e8a20du   /* the meter: max and current, through the data store */
#define H_MAGIC_CUR     0xcbca2f4du
#define H_MAGIC         0xc995bbb8u   /* the wallet current value (kept under its old name) */
#define H_LOADING       0xef2b60c2u

typedef int (*FnGet)(void *, int *, void *, u32);
typedef void (*FnSet)(void *, int, void *, u32);
typedef int (*FnBoolGet)(void *, u8 *, u32);
typedef int (*FnEvent)(void *, int);
typedef int (*FnCount)(void *, const char **, long, long);
typedef void (*FnAdd)(void *, const char **, int, int, int, int, int, int, int);
typedef int (*FnMagicMax)(void *);
typedef int (*FnMagicCur)(void *, int);
typedef int (*FnAge)(void);
typedef u64 (*FnComp)(u64);
typedef void (*FnMagicFill)(u64);

/* Pouch contents. 0-5 are refilled; 6 bow, 7-9 bomb bags, 10 slingshot only show that a container is owned. */
static const char item_names[] =
    "Item_Arrow_Normal\0Item_Bomb\0Item_DekuSeed\0Item_DekuNut\0Item_DekuStick\0Item_Bombchu\0"
    "Weapon_Range_Bow\0Item_BombBag_Small\0Item_BombBag_Medium\0Item_BombBag_Large\0Weapon_Range_SlingShot";

static const char *item_name(int i)
{
    const char *p = item_names;
    while (i-- > 0) while (*p++) {}
    return p;
}

/* Which Link can have it: 1 adult, 2 child, 3 both. */
static int item_flags(int i) { return i == 0 ? 1 : (i == 2 || i == 4) ? 2 : 3; }

/* Diagnostics: log a value only when it changes (and not forever), so the log shows what the game reports. */
static u64 g_trace_last[40];
static u8 g_trace_seen[40];
static int g_trace_lines;
static void trace(const char *label, int id, u64 v)
{
    if (id >= 40 || g_trace_lines > 150 || (g_trace_seen[id] && g_trace_last[id] == v)) return;
    g_trace_seen[id] = 1;
    g_trace_last[id] = v;
    g_trace_lines++;
    dbgx(label, v);
}

static volatile u64 g_link_a;   /* Link's actor object, set every update */
static u32 g_owned[2];      /* items seen in the pouch, per age: they keep being refilled at zero */
static u64 g_last_pm;
static u32 g_ticks;

static int owns_container(void *pm, int first, int last)
{
    FnCount count = (FnCount)(g_base + FN_POUCH_COUNT);
    for (int k = first; k <= last; k++) {
        const char *np[2];
        np[0] = np[1] = item_name(k);
        if (count(pm, np, 0, 0) >= 1) return 1;
    }
    return 0;
}

static void refill_items(void *pm)
{
    u64 b = g_base;
    FnCount count = (FnCount)(b + FN_POUCH_COUNT);
    FnCount cap = (FnCount)(b + FN_POUCH_MAX);
    FnAdd add = (FnAdd)(b + FN_POUCH_ADD);
    int child = ((FnAge)(b + FN_AGE))() != 0;
    for (int i = 0; i < 6; i++) {
        if (!(item_flags(i) & (child ? 2 : 1))) continue;
        const char *np[2];
        np[0] = np[1] = item_name(i);
        int n = count(pm, np, 0, 0);
        trace(i == 0 ? "item arrow count" : i == 1 ? "item bomb count" : i == 2 ? "item seed count" : i == 3 ? "item nut count" : i == 4 ? "item stick count" : "item bombchu count", 10 + i, (u64)(long)n);
        if (n < 0) continue;
        u32 bit = 1u << i;
        if (n != 0) g_owned[child] |= bit;
        if (!(g_owned[child] & bit)) {
            int has = i == 0 ? owns_container(pm, 6, 6) : i == 1 ? owns_container(pm, 7, 9) : i == 2 ? owns_container(pm, 10, 10) : 0;
            if (has) g_owned[child] |= bit;
        }
        if (!(g_owned[child] & bit)) continue;
        int max = cap(pm, np, 0, 0);
        trace(i == 0 ? "item arrow cap" : i == 1 ? "item bomb cap" : i == 2 ? "item seed cap" : i == 3 ? "item nut cap" : i == 4 ? "item stick cap" : "item bombchu cap", 20 + i, (u64)(long)max);
        if (max - n > 0 && max <= 0x3e7) add(pm, np, 0, max - n, 0, 0, 0, 0, 0);
    }
}

static void cheats_world(void)
{
    u64 b = g_base;
    u64 gh = *(u64 *)(b + OFF_GD_HOLDER), eh = *(u64 *)(b + OFF_EV_HOLDER);
    void *pm = *(void **)(b + OFF_PM);
    if (!gh || !eh || !pm) return;
    void *gd = *(void **)gh, *em = *(void **)eh;
    if (!gd || !em) return;
    if ((u64)pm != g_last_pm) { g_last_pm = (u64)pm; g_owned[0] = g_owned[1] = 0; }
    int ev = ((FnEvent)(b + FN_EVENT))(em, 0);
    trace("gate event", 0, (u64)(long)ev);
    if (ev) return;
    u8 loading = 1;
    int lok = ((FnBoolGet)(b + FN_BOOLGET))(gd, &loading, H_LOADING);
    trace("gate loading ok", 1, (u64)lok);
    trace("gate loading flag", 2, loading);
    if (!lok || loading) return;

    void *ctx = (u8 *)pm + POUCH_CTX;
    if (g_cheat[3]) {   /* magic: the pair read through the data-store hashes is (max, current) of the meter */
        int max = 0, cur = 0;
        FnGet get = (FnGet)(b + FN_GET);
        int ok1 = get(gd, &max, ctx, H_MAGIC_MAX);
        int ok2 = get(gd, &cur, ctx, H_MAGIC_CUR);
        trace("magic max", 3, (u64)(long)max);
        trace("magic cur", 4, (u64)(long)cur);
        if (ok1 && max >= 1 && max <= 0x1e0 && ok2 && cur >= 0) {
            /* What the HUD draws lives in a component of Link's actor; refill that first. */
            u64 la = g_link_a;
            if (la) {
                u64 comp = ((FnComp)(b + FN_MAGIC_COMP))(la);
                MemInfo mi;
                if (comp && query_memory(comp, &mi) == 0 && (mi.perm & 1) && comp >= mi.base &&
                    ((mi.base + mi.size - comp) >> 6) >= 0x6f && *(u64 *)comp == b + MAGIC_COMP_VT &&
                    *(u64 *)(comp + 0x18) == la) {
                    int shown = *(int *)(comp + 0x1bbc), busy = *(int *)(comp + 0x1b90);
                    trace("magic shown", 11, (u64)(long)shown);
                    if (shown >= 0 && shown < max && busy <= 0) {
                        ((FnMagicFill)(b + FN_MAGIC_FILL))(comp);
                        trace("magic fill called", 12, 1);
                    }
                } else {
                    trace("magic component not found", 13, comp);
                }
            }
            if (cur < max) {
                ((FnSet)(b + FN_SET))(gd, max, ctx, H_MAGIC_CUR);
                trace("magic set to", 9, (u64)(long)max);
            }
        }
    }
    if (g_cheat[1]) {   /* function-based block: this is the wallet (max 500 in the log) */
        int max = ((FnMagicMax)(b + FN_MAGIC_MAX))(pm);
        int cur = ((FnMagicCur)(b + FN_MAGIC_CUR))(pm, 0);
        trace("wallet max", 7, (u64)(long)max);
        trace("wallet cur", 8, (u64)(long)cur);
        if (max >= 1 && max <= 9999 && cur >= 0 && cur < max) {
            ((FnSet)(b + FN_SET))(gd, max, ctx, H_MAGIC);
            trace("wallet set to", 14, (u64)(long)max);
        }
    }
    if (g_cheat[2]) {
        u64 st = *(u64 *)(b + OFF_STATE);
        if (st && *(u64 *)st && *(u64 *)(b + OFF_ACTORDB)) refill_items(pm);
    }
}

/* ---- teleport (Hookshot's oot_teleport, GPL-3.0) ----------------------- */

#define FN_BOOL2        0x20d9c0ull    /* (gd, u8 *out, hash) -> found */
#define FN_MAKE_ARG     0xd79f10ull    /* (arg buffer) */
#define FN_REQUEST      0xd7a820ull    /* (event manager, arg) */
#define FN_SET_LINK     0x2bc440ull    /* (arg + 0x1950, actor, 0) */
#define FN_REL_LINK     0x20b6f0ull    /* (link) */
#define LINK_VTABLE     0x4974b20ull
#define H_CHILD         0xcea220d3u
#define ARG_SIZE        0x1b20

typedef void (*FnArg)(void *);
typedef u64 (*FnReq)(void *, void *);
typedef void (*FnSetLink)(void *, u64, long);
typedef void (*FnRelLink)(void *);

volatile int g_tp_req;          /* 0 none, else destination index + 1 */
static u64 g_tp_last_ms;
static u8 g_tp_arg[ARG_SIZE + 0x100] __attribute__((aligned(16)));
static char g_tp_str[3][128];          /* the event reads these after the request returns */

static int cpy(char *d, const char *s, int cap)
{
    int n = 0;
    while (s[n] && n + 1 < cap) { d[n] = s[n]; n++; }
    d[n] = 0;
    return s[n] == 0;
}

/* One argument of the request: entries of 200 bytes at arg+8 (type, name buffer, capacity, value). */
static int ev_arg(u8 *arg, const char *name, u32 type, u64 value)
{
    u32 capacity = *(u32 *)arg, count = *(u32 *)(arg + 16);
    if (count >= capacity) return 0;
    u8 *entry = (u8 *)*(u64 *)(arg + 8) + (u64)count * 200;
    char *nbuf = (char *)*(u64 *)(entry + 8);
    u32 ncap = *(u32 *)(entry + 16);
    if (!nbuf || !cpy(nbuf, name, (int)ncap)) return 0;
    if (type == 4) *(entry + 152) = (u8)(value & 1);
    else *(u64 *)(entry + 152) = value;
    *(u32 *)entry = type;
    *(u32 *)(arg + 16) = count + 1;
    return 1;
}

static int do_teleport(u64 link_actor, int idx)
{
    u64 b = g_base;
    if (idx < 0 || idx >= TP_COUNT) return 0;
    u64 now = now_ms();
    if (g_tp_last_ms && now - g_tp_last_ms < 8000) { dbg("[wayfarer] teleport ignored: a request is still in progress\n"); return 0; }
    u64 gh = *(u64 *)(b + OFF_GD_HOLDER), eh = *(u64 *)(b + OFF_EV_HOLDER);
    void *gd = gh ? *(void **)gh : 0, *em = eh ? *(void **)eh : 0;
    if (!gd || !em) { dbg("[wayfarer] teleport unavailable: managers missing\n"); return 0; }
    if (((FnEvent)(b + FN_EVENT))(em, 0)) { dbg("[wayfarer] teleport waits: an event is playing\n"); return 0; }
    u8 loading = 0;
    if (((FnBoolGet)(b + FN_BOOL2))(gd, &loading, H_LOADING) && loading) { dbg("[wayfarer] teleport ignored: a map is loading\n"); return 0; }
    u64 child_word = 1;
    if (!((FnBoolGet)(b + FN_BOOL2))(gd, (u8 *)&child_word, H_CHILD)) child_word = 1;
    int child = (child_word & 1) != 0;

    int base_i = idx * 6 + (child ? 0 : 3);
    const char *banc = nth(tp_dest, base_i), *start = nth(tp_dest, base_i + 1), *boot = nth(tp_dest, base_i + 2);
    if (!*banc) { dbg("[wayfarer] teleport: that place does not exist for this age\n"); return 0; }
    if (!cpy(g_tp_str[0], banc, 128) || !cpy(g_tp_str[1], start, 128) || !cpy(g_tp_str[2], boot, 128)) return 0;

    u8 *arg = g_tp_arg;
    ((FnArg)(b + FN_MAKE_ARG))(arg);
    int ok = 1;
    {
        char *buf = (char *)*(u64 *)(arg + 6800);
        u32 cap = *(u32 *)(arg + 6808);
        if (!buf || !cpy(buf, "Dm_SY_BancChangeGo", (int)cap)) ok = 0;
    }
    ok = ok && ev_arg(arg, "NextBanc", 1, (u64)g_tp_str[0]);
    ok = ok && ev_arg(arg, "NextStartPos", 1, (u64)g_tp_str[1]);
    ok = ok && ev_arg(arg, "BootEventName", 1, (u64)g_tp_str[2]);
    ok = ok && ev_arg(arg, "IsCheckAir", 4, 0);
    if (ok) ((FnSetLink)(b + FN_SET_LINK))(arg + 0x1950, link_actor, 0);
    u64 result = 0;
    if (ok) result = ((FnReq)(b + FN_REQUEST))(em, arg);
    for (int i = 0; i < 2; i++) {
        u8 *l = arg + (i ? 0x1950 : 0x1968);
        *(u64 *)l = b + LINK_VTABLE;
        ((FnRelLink)(b + FN_REL_LINK))(l);
    }
    g_tp_last_ms = now;
    dbgx(ok ? "teleport requested, destination" : "teleport request could not be built, destination", (u64)idx);
    dbgx("teleport request result", result);
    return ok;
}

#include "cheats2.h"
#include "cheats3.h"

/* Called from the player actor's update (owner = its owner object). */
void wf_actor_pre(void *owner)
{
    u64 b = g_base, h = (u64)owner;
    u64 a = *(u64 *)(h + 0x18);
    if (!a || *(u64 *)a != b + 0x4b25e18) return;
    u64 c = *(u64 *)(a + 0x270);
    if (!c) return;
    c = *(u64 *)(c + 0x320);
    if (!c || *(u64 *)(c + 0x18) != a || *(u64 *)(c - 0x1d50) != b + 0x4b4c460) return;
    const u8 *name = (const u8 *)*(u64 *)(c - 0x1d40);
    if (!name || name[0x58] != 'P' || name[0x59] != 'l' || name[0x5a] != 'a' || name[0x5b] != 'y' ||
        name[0x5c] != 'e' || name[0x5d] != 'r' || name[0x5e] != 0) return;

    g_link_tick = now_ms();
    g_link_a = a;

    /* God Mode: Link's life never drops, and is topped up to his current maximum. */
    volatile u64 *life_slot = (volatile u64 *)(b + BRIDGE_OFFSET + 0x70);
    u64 lp = *(u64 *)(h + 0x1c10), mp = *(u64 *)(h + 0x1c18);
    if (g_cheat[0] && lp) {
        *life_slot = lp + 8;
        if (mp) {
            u32 max = *(u32 *)(mp + 8), cur = *(u32 *)(lp + 8);
            if (cur && max <= 0x50 && cur < max)
                __atomic_compare_exchange_n((u32 *)(lp + 8), &cur, max, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        }
    } else {
        *life_slot = 0;
    }

    if (g_tp_req) {
        int req = __atomic_exchange_n(&g_tp_req, 0, __ATOMIC_SEQ_CST);
        if (req) do_teleport(a, req - 1);
    }

    if (g_world_req) {
        int r = __atomic_exchange_n(&g_world_req, 0, __ATOMIC_SEQ_CST);
        if (r == 1) set_time_hour(g_hour);
        else if (r == 2) do_mark(a);
        else if (r == 3) do_return(a);
        else if (r == 4) do_age(a);
    }
    if (g_item_req) {
        int r = __atomic_exchange_n(&g_item_req, 0, __ATOMIC_SEQ_CST) - 1;
        if (game_ready()) item_edit(r >> 8, r & 255);
    }
    if (g_cheat[4]) flight_step(a);
    else if (g_speed) speed_step(a);
    else g_fl_actor = g_sp_actor = 0;
    if (g_open && g_tab == 1 && (++g_icnt_tick % 20 == 0 || g_icnt_dirty)) { g_icnt_dirty = 0; refresh_counts(); }

    if (!(g_cheat[1] | g_cheat[2] | g_cheat[3])) return;
    if (++g_ticks % (g_cheat[3] ? 3 : 10)) return;   /* the magic meter needs a quicker rhythm */
    cheats_world();
}

u64 wf_actor_orig;   /* where the game's own actor update continues */

__asm__(
    ".text\n"
    ".global wf_actor_thunk\n"
    "wf_actor_thunk:\n"
    "  sub  sp, sp, #0x60\n"
    "  stp  x0, x1, [sp]\n"
    "  stp  x2, x3, [sp, #16]\n"
    "  stp  x4, x5, [sp, #32]\n"
    "  stp  x6, x7, [sp, #48]\n"
    "  stp  x29, x30, [sp, #64]\n"
    "  str  x8, [sp, #80]\n"
    "  bl   wf_actor_pre\n"
    "  ldp  x0, x1, [sp]\n"
    "  ldp  x2, x3, [sp, #16]\n"
    "  ldp  x4, x5, [sp, #32]\n"
    "  ldp  x6, x7, [sp, #48]\n"
    "  ldp  x29, x30, [sp, #64]\n"
    "  ldr  x8, [sp, #80]\n"
    "  add  sp, sp, #0x60\n"
    "  adrp x16, wf_actor_orig\n"
    "  ldr  x16, [x16, :lo12:wf_actor_orig]\n"
    "  br   x16\n");
extern char wf_actor_thunk[];
