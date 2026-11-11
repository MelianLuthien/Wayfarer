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

static int item_count_of(int item)
{
    const char *name = nth(item_pouch, item);
    char alias[64];
    int n = pouch_count_name(name);
    if (!mask_alias(name, alias)) return n;
    int m = pouch_count_name(alias);
    return n > m ? n : m;
}

static u32 g_icnt_tick;
static volatile int g_icnt_dirty;

static void refresh_counts(void)
{
    if (!game_ready()) return;
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
    dbgx("item edit, item", (u64)item);
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
