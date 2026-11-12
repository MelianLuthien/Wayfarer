/*
 * A 16-byte settings file on the SD card, so the Teleport favorites and sort survive a restart.
 *
 * The module has no libraries, so this speaks the Switch's service protocol (CMIF) by hand:
 * connect to "sm:", ask it for "fsp-srv", open the SD card's file system and read or write
 * /wayfarer/favorites.bin. Every step is allowed to fail: then nothing is saved and the menu simply
 * keeps the favorites in memory, as before. Only the menu's input thread calls this, never the game's.
 * GPL-3.0.
 */

static u32 g_sd_fs;       /* IFileSystem of the SD card, 0 = not open */
static int g_sd_tries;    /* connection attempts so far; gives up after a few */

static u32 *ipc_msg(void)
{
    u64 t;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(t));
    return (u32 *)t;
}

static u32 svc_connect(const char *name, u32 *h)
{
    register u64 x0 __asm__("x0") = 0;
    register u64 x1 __asm__("x1") = (u64)name;
    __asm__ volatile("svc #0x1f" : "+r"(x0), "+r"(x1) : : "x2", "x3", "x4", "x5", "x6", "x7", "memory");
    *h = (u32)x1;
    return (u32)x0;
}

static u32 svc_request(u32 h)
{
    register u64 x0 __asm__("x0") = h;
    __asm__ volatile("svc #0x21" : "+r"(x0) : : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "memory");
    return (u32)x0;
}

static void svc_close(u32 h)
{
    register u64 x0 __asm__("x0") = h;
    __asm__ volatile("svc #0x19" : "+r"(x0) : : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "memory");
}

static char g_sd_path[0x301];

/* One request to a service. in: raw bytes after the command; pid: send our process id; path: a path
 * (pointer descriptor); buf: a data buffer to send (buf_out 0) or to receive into (buf_out 1);
 * *mv: the handle the service returns, if any; out: copy of the reply's raw bytes. Returns 0 or an error. */
static u32 ipc(u32 h, u32 cmd, const void *in, u32 in_len, int pid, int path, void *buf, u32 buf_len, int buf_out,
               u32 *mv, void *out, u32 out_len)
{
    u32 *m = ipc_msg();
    for (int i = 0; i < 64; i++) m[i] = 0;
    int nx = path ? 1 : 0, na = (buf && !buf_out) ? 1 : 0, nb = (buf && buf_out) ? 1 : 0;
    m[0] = 4u | ((u32)nx << 16) | ((u32)na << 20) | ((u32)nb << 24);
    int w = 2;
    if (pid) {
        m[1] |= 0x80000000u;
        m[w++] = 1;      /* send process id */
        m[w++] = 0;
        m[w++] = 0;
    }
    if (path) {
        u64 a = (u64)g_sd_path;
        m[w++] = (u32)(((a >> 36) & 0x3f) << 6) | (u32)(((a >> 32) & 0xf) << 12) | (0x301u << 16);
        m[w++] = (u32)a;
    }
    if (buf) {
        u64 a = (u64)buf;
        m[w++] = buf_len;
        m[w++] = (u32)a;
        m[w++] = 1u | (u32)((a >> 36) << 2) | (u32)(((a >> 32) & 0xf) << 28);   /* mode 1: non-secure */
    }
    int pad = (4 - (w & 3)) & 3;   /* the raw part starts on a 16-byte boundary */
    w += pad;
    u32 words = (in_len + 3) / 4;
    m[1] |= (u32)(pad + 4 + (int)words + 4);
    m[w] = 0x49434653u;            /* "SFCI" */
    m[w + 2] = cmd;
    {
        const u8 *s = (const u8 *)in;
        u8 *d = (u8 *)&m[w + 4];
        for (u32 i = 0; i < in_len; i++) d[i] = s[i];
    }
    u32 r = svc_request(h);
    if (r) return r;
    u32 h0 = m[0], h1 = m[1];
    int p = 2;
    if (h1 >> 31) {
        u32 hd = m[p++];
        if (hd & 1) p += 2;
        int ncopy = (int)((hd >> 1) & 15), nmove = (int)((hd >> 5) & 15);
        p += ncopy;
        if (nmove && mv) *mv = m[p];
        p += nmove;
    }
    p += (int)((h0 >> 16) & 15) * 2 + (int)((h0 >> 20) & 15) * 3 + (int)((h0 >> 24) & 15) * 3 + (int)((h0 >> 28) & 15) * 3;
    p = (p + 3) & ~3;
    if (m[p] != 0x4f434653u) return 0x7777;   /* not "SFCO" */
    if (m[p + 2]) return m[p + 2];
    if (out) {
        const u8 *s = (const u8 *)&m[p + 4];
        u8 *d = (u8 *)out;
        for (u32 i = 0; i < out_len; i++) d[i] = s[i];
    }
    return 0;
}

static void sd_set_path(const char *s)
{
    int i = 0;
    for (; s[i]; i++) g_sd_path[i] = s[i];
    for (; i < 0x301; i++) g_sd_path[i] = 0;
}

static u32 sd_fs(void)
{
    if (g_sd_fs) return g_sd_fs;
    if (g_sd_tries >= 3) return 0;
    g_sd_tries++;
    u32 sm = 0, srv = 0, fs = 0;
    u64 zero = 0, name = 0;
    const char *n = "fsp-srv";
    for (int i = 0; n[i]; i++) name |= (u64)(u8)n[i] << (8 * i);
    u32 r = svc_connect("sm:", &sm);
    if (r) { dbgx("sd: no sm", r); return 0; }
    r = ipc(sm, 0, &zero, 8, 1, 0, 0, 0, 0, 0, 0, 0);
    if (!r) r = ipc(sm, 1, &name, 8, 0, 0, 0, 0, 0, &srv, 0, 0);
    svc_close(sm);
    if (r || !srv) { dbgx("sd: no fsp-srv", r); return 0; }
    r = ipc(srv, 1, &zero, 8, 1, 0, 0, 0, 0, 0, 0, 0);
    if (!r) r = ipc(srv, 18, 0, 0, 0, 0, 0, 0, 0, &fs, 0, 0);
    svc_close(srv);
    if (r || !fs) { dbgx("sd: no SD card", r); return 0; }
    g_sd_fs = fs;
    return fs;
}

static u32 sd_open(u32 fs, int mode, u32 *file)
{
    u32 m = (u32)mode;
    sd_set_path("/wayfarer/favorites.bin");
    return ipc(fs, 8, &m, 4, 0, 1, 0, 0, 0, file, 0, 0);
}

/* Reads the 16-byte file into d[0..3]; returns 1 if it was there. */
static int sd_load(u32 *d)
{
    u32 fs = sd_fs(), f = 0;
    if (!fs) return 0;
    if (sd_open(fs, 1, &f) || !f) return 0;
    u32 in[6] = {0, 0, 0, 0, 16, 0};   /* option, pad, offset (u64), size (u64) */
    u64 got = 0;
    u32 r = ipc(f, 0, in, 24, 0, 0, d, 16, 1, 0, &got, 8);
    svc_close(f);
    if (r || got != 16) { dbgx("sd: read failed", r); return 0; }
    return 1;
}

static void sd_save(const u32 *d)
{
    u32 fs = sd_fs(), f = 0;
    if (!fs) return;
    sd_set_path("/wayfarer");
    ipc(fs, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0);                  /* the folder; fine if it is there */
    u32 cf[4] = {0, 0, 16, 0};                                 /* option, pad, size (u64) */
    sd_set_path("/wayfarer/favorites.bin");
    ipc(fs, 0, cf, 16, 0, 1, 0, 0, 0, 0, 0, 0);                /* the file; fine if it is there */
    u32 r = sd_open(fs, 2, &f);
    if (r || !f) { dbgx("sd: cannot open", r); return; }
    u32 in[6] = {1, 0, 0, 0, 16, 0};                           /* flush after writing */
    u32 tmp[4] = {d[0], d[1], d[2], d[3]};
    r = ipc(f, 1, in, 24, 0, 0, tmp, 16, 0, 0, 0, 0);
    svc_close(f);
    if (r) dbgx("sd: write failed", r);
}
