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
#define POUCH_CTX       0x3bc0ull      /* added to the pouch manager to get the data context */
#define H_WALLET_MAX    0x39e8a20du
#define H_RUPEES        0xcbca2f4du
#define H_MAGIC         0xc995bbb8u
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
    if (g_cheat[1]) {
        int max = 0, cur = 0;
        FnGet get = (FnGet)(b + FN_GET);
        int ok1 = get(gd, &max, ctx, H_WALLET_MAX);
        int ok2 = get(gd, &cur, ctx, H_RUPEES);
        trace("rupee max", 3, (u64)(long)max);
        trace("rupee cur", 4, (u64)(long)cur);
        trace("rupee get ok", 5, (u64)(ok1 * 2 + ok2));
        if (ok1 && max >= 1 && max <= 0x1e0 && ok2 && cur >= 0 && cur < max) {
            ((FnSet)(b + FN_SET))(gd, max, ctx, H_RUPEES);
            trace("rupee set to", 6, (u64)(long)max);
        }
    }
    if (g_cheat[3]) {
        int max = ((FnMagicMax)(b + FN_MAGIC_MAX))(pm);
        int cur = ((FnMagicCur)(b + FN_MAGIC_CUR))(pm, 0);
        trace("magic max", 7, (u64)(long)max);
        trace("magic cur", 8, (u64)(long)cur);
        if (max >= 1 && max <= 9999 && cur >= 0 && cur < max) {
            ((FnSet)(b + FN_SET))(gd, max, ctx, H_MAGIC);
            trace("magic set to", 9, (u64)(long)max);
        }
    }
    if (g_cheat[2]) {
        u64 st = *(u64 *)(b + OFF_STATE);
        if (st && *(u64 *)st && *(u64 *)(b + OFF_ACTORDB)) refill_items(pm);
    }
}

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

    if (!(g_cheat[1] | g_cheat[2] | g_cheat[3])) return;
    if (++g_ticks % 10) return;   /* a few times a second */
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
