/* Inventory editor, time, mark/return position and age switch. Included by cheats.h after do_teleport.
 * These follow Hookshot's cheats (GPL-3.0); its offsets minus 0x80020000 are the ones used here. */

#define OFF_PS          0x4c7e0d0ull   /* -> holder -> pouch singleton */
#define OFF_ACTORINFO   0x4e6c060ull
#define OFF_WORLD       0x4c7e898ull   /* -> holder -> world manager */
#define FN_POUCH_EDIT   0x28b4a70ull   /* OneShotAddOrRemovePouchContent(event) */
#define SCRIPT_VT       0x3eea6d8ull
#define FN_SET_TIME     0x34522d0ull   /* (world manager, seconds since midnight) */
#define FN_GD_STRING    0xcb0ea0ull    /* (gd, u64 *out, hash) -> found; out = text pointer */
#define FN_CHANGE_POS   0x6b2530ull    /* (controller, float[4]) */
#define CTL_VT          0x4c34f10ull
#define H_BANC          0x1d6189dau    /* Sequence_CurrentBanc */

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int seq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

/* No cutscene playing and no map loading. */
static int game_ready(void)
{
    u64 b = g_base;
    u64 gh = *(u64 *)(b + OFF_GD_HOLDER), eh = *(u64 *)(b + OFF_EV_HOLDER);
    if (!gh || !eh || !*(u64 *)(b + OFF_PM)) return 0;
    void *gd = *(void **)gh, *em = *(void **)eh;
    if (!gd || !em) return 0;
    if (((FnEvent)(b + FN_EVENT))(em, 0)) return 0;
    u8 loading = 1;
    if (!((FnBoolGet)(b + FN_BOOLGET))(gd, &loading, H_LOADING) || loading) return 0;
    return 1;
}

static u8 g_ev[0x100 + 16] __attribute__((aligned(16)));

/* Adds (delta > 0) or removes (delta < 0) pouch content by name, as the game's own event does. -1 when not ready. */
static int pouch_change(const char *name, int delta)
{
    u64 b = g_base;
    u64 ps = *(u64 *)(b + OFF_PS), st = *(u64 *)(b + OFF_STATE);
    if (!ps || !*(u64 *)ps || !st || !*(u64 *)st || !*(u64 *)(b + OFF_ACTORDB) || !*(u64 *)(b + OFF_ACTORINFO) ||
        !delta || slen(name) >= 0x60) return -1;
    u8 *ev = g_ev;
    for (int i = 0; i < 0xe0; i++) ev[i] = 0;
    cpy((char *)ev + 0x80, name, 0x60);
    *(u64 *)(ev + 0x20) = (u64)ev + 0x60;
    *(u64 *)(ev + 0x30) = (u64)ev + 0x70;
    *(u64 *)(ev + 0x38) = (u64)ev + 0x68;
    *(u64 *)(ev + 0x48) = (u64)ev + 0x74;
    *(u64 *)(ev + 0x50) = (u64)ev + 0x74;
    *(u64 *)(ev + 0x60) = (u64)ev + 0x80;
    *(u64 *)(ev + 0x68) = b + SCRIPT_VT;
    *(u32 *)(ev + 0x70) = (u32)delta;
    ((void (*)(void *))(b + FN_POUCH_EDIT))(ev);
    return (int)(*(u32 *)(ev + 0x18) >> 30);
}

static int pouch_count_name(const char *name)
{
    void *pm = *(void **)(g_base + OFF_PM);
    if (!pm || slen(name) >= 0x60) return -1;
    const char *np[2];
    np[0] = np[1] = name;
    return ((FnCount)(g_base + FN_POUCH_COUNT))(pm, np, 0, 0);
}

/* Masks share one slot; the game takes "Item_Mask_X" when adding one and may keep it as "Armor_Child_Mask_X". */
static int mask_alias(const char *name, char *alias)
{
    static const char pre[] = "Armor_Child_Mask_";
    for (int i = 0; i < 17; i++) if (name[i] != pre[i]) return 0;
    cpy(alias, "Item_Mask_", 64);
    cpy(alias + 10, name + 17, 54);
    return 1;
}

/* The game's pouch count does not seem to report songs, so remember the ones this menu gave or took. */
static volatile u8 g_song_mem[ITEM_COUNT];

static int item_count_of(int item)
{
    const char *name = nth(item_pouch, item);
    char alias[64];
    int n = pouch_count_name(name);
    if (item >= 70 && item < 82) return n > 0 ? n : g_song_mem[item];
    if (!mask_alias(name, alias)) return n;
    int m = pouch_count_name(alias);
    return n > m ? n : m;
}

static u32 g_icnt_tick;
static volatile int g_icnt_dirty;

/* One-time probe (v1.1 test build): where does the game keep "I know this song"? Logs, per song, whether a few
 * candidate names exist as game-data flags (and their value) and whether the pouch's hash map knows the name. */
#define FN_HASH         0x1739e0ull    /* (const char *, 0) -> murmur3 hash */
static int g_probe_done;

static void probe_name(const char *pre, const char *song, const char *label)
{
    char nm[96];
    int n = 0;
    for (; pre[n] && n < 40; n++) nm[n] = pre[n];
    for (int k = 0; song[k] && n < 90; k++) nm[n++] = song[k];
    nm[n] = 0;
    u64 b = g_base, gh = *(u64 *)(b + OFF_GD_HOLDER);
    void *gd = gh ? *(void **)gh : 0, *pm = *(void **)(b + OFF_PM);
    u32 h = ((u32 (*)(const char *, int))(b + FN_HASH))(nm, 0);
    u8 v1 = 0, v2 = 0;
    u64 r = 0;
    if (gd) {
        r |= (u64)(((FnBoolGet)(b + FN_BOOLGET))(gd, &v1, h) & 1);
        r |= (u64)v1 << 8;
        r |= (u64)(((FnBoolGet)(b + FN_BOOL2))(gd, &v2, h) & 1) << 16;
        r |= (u64)v2 << 24;
    }
    u64 mp = 0xffff;
    if (pm) {
        u64 t = *(u64 *)((u64)pm + 0x3bb0);
        u64 arr = t ? *(u64 *)(t + 8) : 0;
        u32 sz = t ? *(u32 *)(t + 0x14) : 0;
        if (arr && sz) {
            u32 slot = h % sz;
            for (u32 i = 0; i < sz; i++, slot = (slot + 1) % sz) {
                u32 key = *(u32 *)(arr + slot * 8ull);
                if (key == h) { mp = *(u32 *)(arr + slot * 8ull + 4); break; }
                if (!key) break;
            }
        }
    }
    dbgx(label, r | (mp << 32));
}

static void probe_songs(void)
{
    static const char sn[] = "ZeldaLullaby\0EponaSong\0SariaSong\0SunSong\0TimeSong\0StormSong\0ForestMinuet\0FireBolero\0"
                             "WaterSerenade\0ShadowNocturne\0SpiritRequiem\0LightPrelude\0";
    for (int i = 0; i < 12; i++) {
        const char *s = nth(sn, i);
        dbgx("probe song", (u64)i);
        probe_name("Item_Melody_", s, "probe Item_Melody_ (pm<<32 | b2v<<24 | b2f<<16 | bv<<8 | bf)");
        probe_name("OcarinaMelody", s, "probe OcarinaMelody");
        probe_name("Melody_", s, "probe Melody_");
        probe_name("IsGet_Melody_", s, "probe IsGet_Melody_");
        probe_name("Learned", s, "probe Learned");
    }
    dbgx("probe done, pouch map size", (u64)*(u32 *)(*(u64 *)((u64)*(void **)(g_base + OFF_PM) + 0x3bb0) + 0x14));
}


/* Song probe 2: what changes in memory when a song is added or removed? Snapshot before, diff after. */
#define SNAP_PM 0x3c00
#define SNAP_GD 8192
static u64 g_pm_snap[SNAP_PM / 8];
static u8 g_gd_snap[SNAP_GD];
static u64 g_int_snap[4096 * 4];
static int g_snap_have, g_snap_pending;

static u32 gd_int_count(void)
{
    u64 gh = *(u64 *)(g_base + OFF_GD_HOLDER);
    u64 gd = gh ? *(u64 *)gh : 0;
    if (!gd || !*(u64 *)(gd + 0xc18)) return 0;
    u32 n = *(u32 *)(gd + 0xc10);
    return n > 4096 ? 4096 : n;
}

static u32 gd_bool_count(void)
{
    u64 gh = *(u64 *)(g_base + OFF_GD_HOLDER);
    u64 gd = gh ? *(u64 *)gh : 0;
    if (!gd || !*(u64 *)(gd + 24)) return 0;
    u32 n = *(u32 *)(gd + 16);
    return n > SNAP_GD ? SNAP_GD : n;
}

static void snap_take(void)
{
    u64 pm = *(u64 *)(g_base + OFF_PM);
    if (!pm) return;
    for (int i = 0; i < SNAP_PM / 8; i++) g_pm_snap[i] = *(u64 *)(pm + i * 8ull);
    u64 gd = *(u64 *)*(u64 *)(g_base + OFF_GD_HOLDER);
    u32 n = gd_bool_count();
    for (u32 i = 0; i < n; i++) g_gd_snap[i] = *(u8 *)(*(u64 *)(gd + 24) + i * 24ull + 22);
    {
        u32 m = gd_int_count();
        u64 base = *(u64 *)(gd + 0xc18);
        for (u32 i = 0; i < m * 4; i++) g_int_snap[i] = *(u64 *)(base + i * 8ull);
    }
    g_snap_have = 1;
}

static void snap_diff(void)
{
    u64 pm = *(u64 *)(g_base + OFF_PM);
    if (!pm || !g_snap_have) return;
    int shown = 0;
    for (int i = 0; i < SNAP_PM / 8 && shown < 24; i++) {
        u64 now = *(u64 *)(pm + i * 8ull);
        if (now == g_pm_snap[i]) continue;
        dbgx("snap pm offset", (u64)i * 8);
        dbgx("snap pm old", g_pm_snap[i]);
        dbgx("snap pm new", now);
        shown++;
    }
    u64 gd = *(u64 *)*(u64 *)(g_base + OFF_GD_HOLDER);
    u32 n = gd_bool_count();
    shown = 0;
    for (u32 i = 0; i < n && shown < 24; i++) {
        u64 e = *(u64 *)(gd + 24) + i * 24ull;
        u8 now = *(u8 *)(e + 22);
        if (now == g_gd_snap[i]) continue;
        dbgx("snap flag index", (u64)i);
        dbgx("snap flag entry first 8 bytes", *(u64 *)e);
        dbgx("snap flag now", now);
        shown++;
    }
    dbg("[wayfarer] snap diff end\n");
}

static void refresh_counts(void)
{
    if (!game_ready()) return;
    if (g_snap_pending) { g_snap_pending--; snap_diff(); }
    for (int i = 0; i < ITEM_COUNT; i++) {
        int c = item_count_of(i);
        if (c != g_icount[i]) { g_icount[i] = c; g_gen++; }
    }
}

/* op 200: unlock (add one) or relock (remove all); op 0..127: add op - 64. */
static void item_edit(int item, int op)
{
    if (item < 0 || item >= ITEM_COUNT) return;
    const char *name = nth(item_pouch, item);
    char alias[64];
    int is_mask = mask_alias(name, alias);
    int have = item_count_of(item);
    int delta = 0;
    if (op == 200) delta = have > 0 ? -have : 1;
    else if (op < 128) delta = op - 64;
    if (!delta || have < 0) return;
    if (is_mask && delta > 0) {   /* give back whichever mask is in the slot first */
        for (int i = 0; i < ITEM_COUNT; i++) {
            const char *on = nth(item_pouch, i);
            char oa[64];
            if (i == item || !mask_alias(on, oa)) continue;
            const char *sp[2];
            sp[0] = on;
            sp[1] = oa;
            for (int k = 0; k < 2; k++) {
                int held = pouch_count_name(sp[k]);
                if (held > 0) pouch_change(sp[k], -held);
            }
        }
        delta = 1;
    }
    int result = -1;
    if (item >= 70 && item < 82) snap_take();
    if (is_mask && delta < 0) {
        const char *sp[2];
        sp[0] = name;
        sp[1] = alias;
        for (int k = 0; k < 2; k++) {
            int held = pouch_count_name(sp[k]);
            if (held > 0) result = pouch_change(sp[k], -held);
        }
    } else {
        result = pouch_change(is_mask ? alias : name, delta);
    }
    if (item >= 70 && item < 82) { snap_diff(); g_snap_pending = 4; }
    if (item >= 70 && item < 82 && result != -1) g_song_mem[item] = delta > 0 ? 1 : 0;
    dbgx("item edit, item", (u64)item);
    if (item >= 70 && item < 82) dbgx("item edit, raw pouch count", (u64)(long)pouch_count_name(name));
    dbgx("item edit, status", (u64)(long)result);
    g_icnt_dirty = 1;
}

static void set_time_hour(int hour)
{
    u64 b = g_base;
    u64 h = *(u64 *)(b + OFF_WORLD);
    u64 world = h ? *(u64 *)h : 0;
    if (!world || !*(u64 *)(world + 0x510) || hour < 0 || hour > 23) { dbg("[wayfarer] world clock unavailable\n"); return; }
    ((void (*)(u64, u64))(b + FN_SET_TIME))(world, (u64)hour * 3600);
    dbgx("time set to hour", (u64)hour);
}

/* Link's movement controller, found the way OneShotPhysicsCctChangePosition does. */
static u64 link_controller(u64 a)
{
    static const u8 off[4] = {80, 32, 16, 8};
    u64 c = *(u64 *)(a + 0x270);
    for (int i = 0; i < 4 && c; i++) c = *(u64 *)(c + off[i]);
    if (!c) return 0;
    u64 vt = *(u64 *)c;
    if (vt != g_base + CTL_VT || *(u64 *)(vt + 0xe8) != g_base + FN_CHANGE_POS) return 0;
    return c;
}

static u32 g_mark[4] __attribute__((aligned(16)));
static u64 g_mark_ctl;

static void do_mark(u64 a)
{
    u64 c = link_controller(a);
    if (!c) { dbg("[wayfarer] mark: controller not found\n"); return; }
    for (int i = 0; i < 3; i++) g_mark[i] = *(u32 *)(a + 796 + i * 4);
    g_mark[3] = 0;
    g_mark_ctl = c;
    dbg("[wayfarer] position marked\n");
}

static void do_return(u64 a)
{
    u64 c = link_controller(a);
    if (!c || c != g_mark_ctl) { dbg("[wayfarer] return: no mark on this map\n"); return; }
    static u32 arg[4] __attribute__((aligned(16)));
    for (int i = 0; i < 4; i++) arg[i] = g_mark[i];
    ((void (*)(u64, u32 *))(g_base + FN_CHANGE_POS))(c, arg);
    dbg("[wayfarer] returned to the mark\n");
}

/* The game-data bool table: hash -> index at mgr+32 (size at +44), 24-byte entries at mgr+24 with the value at +22. */
static int gd_set_bool(void *gd, u32 hash, int value)
{
    u64 mgr = (u64)gd;
    u64 table = *(u64 *)(mgr + 32);
    u32 size = *(u32 *)(mgr + 44);
    if (!table || !size) return 0;
    for (u32 probe = 0, slot = hash % size; probe < size; probe++, slot = (slot + 1) % size) {
        u32 key = *(u32 *)(table + slot * 8ull);
        if (!key) return 0;
        if (key != hash) continue;
        u32 index = *(u32 *)(table + slot * 8ull + 4), count = *(u32 *)(mgr + 16);
        if (index >= count) return 0;
        *(u8 *)(*(u64 *)(mgr + 24) + index * 24ull + 22) = value ? 1 : 0;
        return 1;
    }
    return 0;
}

/* Age: flips Sequence_IsChildhood and reloads this map (or the Market) through the teleport. */
static void do_age(u64 a)
{
    u64 b = g_base;
    u64 gh = *(u64 *)(b + OFF_GD_HOLDER);
    void *gd = gh ? *(void **)gh : 0;
    if (!gd) return;
    u64 child_word = 1;
    if (!((FnBoolGet)(b + FN_BOOL2))(gd, (u8 *)&child_word, H_CHILD)) { dbg("[wayfarer] age: flag not found\n"); return; }
    int child = (child_word & 1) != 0;
    char current[80];
    current[0] = 0;
    u64 text = 0;
    if (((int (*)(void *, u64 *, u32))(b + FN_GD_STRING))(gd, &text, H_BANC) && text) cpy(current, (const char *)text, 80);
    int target = 3;   /* the Market holds the Temple of Time */
    for (int i = 0; i < TP_COUNT; i++) {
        const char *cb = nth(tp_dest, i * 6), *ab = nth(tp_dest, i * 6 + 3);
        const char *other = child ? ab : cb;
        if (*other && current[0] && (seq(cb, current) || seq(ab, current))) { target = i; break; }
    }
    if (!gd_set_bool(gd, H_CHILD, !child)) { dbg("[wayfarer] age: could not write the flag\n"); return; }
    dbgx("age switch, via destination", (u64)target);
    if (!do_teleport(a, target)) {
        gd_set_bool(gd, H_CHILD, child);
        dbg("[wayfarer] age switch undone: the map change was not requested\n");
    }
}
