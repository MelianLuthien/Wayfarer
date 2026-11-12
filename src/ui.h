/*
 * The menu's look: software drawing into the panel buffer and paint_panel(). Plain C with no
 * hardware calls, so tools/uitest.c can compile it on the host and write the panel to an image.
 *
 * The panel is 540 x 340 logical pixels over a pre-made carved-wood frame and parchment
 * (build/art/panel.bin, 1080 x 680). Text is Cormorant, drawn from bitmaps made at twice the
 * logical size (build/art/fonts.bin). Everything else (selection boxes, tabs, toggles) is drawn
 * here with anti-aliased rounded shapes.
 *
 * The including file provides: u8, the menu state (g_tab, g_sel, ...), the label blobs, nth(),
 * the icon art (wf_icon_blob, ICON_*), font.h / panel.h and wf_font_blob / wf_panel_blob.
 */

/* One fixed logical size; only the pixel scale follows the screen. */
#define PW 540
#define PH 340

static u8 *g_px;       /* panel pixels (CPU copy) */
static int g_pitch;    /* bytes per row */
static int g_S;        /* integer scale */
static int g_wpx, g_hpx;
static int g_ri, g_bi; /* byte index of red and blue */

#define CR(c) (((c) >> 16) & 255)
#define CG(c) (((c) >> 8) & 255)
#define CB(c) ((c) & 255)

/* ink and wood colours, 0xRRGGBB */
#define INK    0x2a1d12
#define DESC   0x5a4128
#define GREEN  0x3d5e28
#define WOOD   0x4a3522
#define BRASS  0xa8893f
#define GOLDC  0xd9bd6a
#define FOOTC  0x3a2a1a
#define CREAMC 0xf3ead2

static void blend(int x, int y, int r, int g, int b, int a)
{
    if (x < 0 || y < 0 || x >= g_wpx || y >= g_hpx || a <= 0) return;
    u8 *p = g_px + y * g_pitch + x * 4;
    int ia = 255 - a;
    p[g_ri] = (u8)((p[g_ri] * ia + r * a) / 255);
    p[1] = (u8)((p[1] * ia + g * a) / 255);
    p[g_bi] = (u8)((p[g_bi] * ia + b * a) / 255);
    p[3] = 255;
}

static void fill(int lx, int ly, int lw, int lh, u32 c, int a)
{
    int S = g_S;
    for (int y = ly * S; y < (ly + lh) * S; y++)
        for (int x = lx * S; x < (lx + lw) * S; x++) blend(x, y, CR(c), CG(c), CB(c), a);
}

/* ---- background ---------------------------------------------------------- */

/* The frame art is twice the logical size, so device pixel i looks at source (i + .5) * 2 / S - .5. */
static void paint_bg(void)
{
    int S = g_S;
    for (int y = 0; y < g_hpx; y++) {
        int sy = ((2 * y + 1) * 256) / S - 128;
        int y0 = sy >> 8, fy = sy & 255, y1 = y0 + 1;
        if (y0 < 0) y0 = 0;
        if (y1 < 0) y1 = 0;
        if (y0 > PANEL_SRC_H - 1) y0 = PANEL_SRC_H - 1;
        if (y1 > PANEL_SRC_H - 1) y1 = PANEL_SRC_H - 1;
        u8 *row = g_px + y * g_pitch;
        for (int x = 0; x < g_wpx; x++) {
            int sx = ((2 * x + 1) * 256) / S - 128;
            int x0 = sx >> 8, fx = sx & 255, x1 = x0 + 1;
            if (x0 < 0) x0 = 0;
            if (x1 < 0) x1 = 0;
            if (x0 > PANEL_SRC_W - 1) x0 = PANEL_SRC_W - 1;
            if (x1 > PANEL_SRC_W - 1) x1 = PANEL_SRC_W - 1;
            const u8 *a = wf_panel_blob + (y0 * PANEL_SRC_W + x0) * 3, *b = wf_panel_blob + (y0 * PANEL_SRC_W + x1) * 3;
            const u8 *c = wf_panel_blob + (y1 * PANEL_SRC_W + x0) * 3, *d = wf_panel_blob + (y1 * PANEL_SRC_W + x1) * 3;
            int w00 = (256 - fx) * (256 - fy), w10 = fx * (256 - fy), w01 = (256 - fx) * fy, w11 = fx * fy;
            u8 *p = row + x * 4;
            p[g_ri] = (u8)((a[0] * w00 + b[0] * w10 + c[0] * w01 + d[0] * w11) >> 16);
            p[1] = (u8)((a[1] * w00 + b[1] * w10 + c[1] * w01 + d[1] * w11) >> 16);
            p[g_bi] = (u8)((a[2] * w00 + b[2] * w10 + c[2] * w01 + d[2] * w11) >> 16);
            p[3] = 255;
        }
    }
}

/* ---- shapes -------------------------------------------------------------- */

static float fsq(float x)
{
    if (x <= 0.0f) return 0.0f;
    float y = x > 1.0f ? x : 1.0f;
    for (int i = 0; i < 6; i++) y = 0.5f * (y + x / y);
    return y;
}

static int lerp8(int a, int b, float t) { return a + (int)((float)(b - a) * t); }

/* Rounded rectangle at logical (x, y), size w x h, corner radius rad. th > 0 draws only a ring of
 * that thickness inside the edge. The fill runs from colour c0 at the top to c1 at the bottom. */
static void rr(float x, float y, float w, float h, float rad, float th, u32 c0, u32 c1, int a)
{
    int S = g_S;
    int ix0 = (int)((x - 1.0f) * S), ix1 = (int)((x + w + 1.0f) * S) + 1;
    int iy0 = (int)((y - 1.0f) * S), iy1 = (int)((y + h + 1.0f) * S) + 1;
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > g_wpx) ix1 = g_wpx;
    if (iy1 > g_hpx) iy1 = g_hpx;
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f - rad, hy = h * 0.5f - rad, inv = 1.0f / (float)S;
    for (int j = iy0; j < iy1; j++) {
        float Y = ((float)j + 0.5f) * inv, py = Y - cy, t = (Y - y) / h;
        if (py < 0) py = -py;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        float qy = py - hy;
        int r = lerp8(CR(c0), CR(c1), t), g = lerp8(CG(c0), CG(c1), t), b = lerp8(CB(c0), CB(c1), t);
        for (int i = ix0; i < ix1; i++) {
            float px = ((float)i + 0.5f) * inv - cx;
            if (px < 0) px = -px;
            float qx = px - hx, sd;
            if (qx > 0 && qy > 0) sd = fsq(qx * qx + qy * qy) - rad;
            else sd = (qx > qy ? qx : qy) - rad;
            float co = 0.5f - sd * (float)S;
            if (co <= 0) continue;
            if (co > 1) co = 1;
            if (th > 0) {
                float ci = 0.5f + (-th - sd) * (float)S;
                if (ci >= 1) continue;
                if (ci > 0) co *= 1.0f - ci;
            }
            blend(i, j, r, g, b, (int)((float)a * co));
        }
    }
}

/* A shaded ball: light from the upper left (colour hi), through mid, to lo at the far edge. */
static void disc(float cx, float cy, float rad, u32 hi, u32 mid, u32 lo, int a)
{
    int S = g_S;
    float inv = 1.0f / (float)S;
    int ix0 = (int)((cx - rad - 1) * S), ix1 = (int)((cx + rad + 1) * S) + 1;
    int iy0 = (int)((cy - rad - 1) * S), iy1 = (int)((cy + rad + 1) * S) + 1;
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    for (int j = iy0; j < iy1; j++)
        for (int i = ix0; i < ix1; i++) {
            float X = ((float)i + 0.5f) * inv - cx, Y = ((float)j + 0.5f) * inv - cy;
            float sd = fsq(X * X + Y * Y) - rad, co = 0.5f - sd * (float)S;
            if (co <= 0) continue;
            if (co > 1) co = 1;
            float hxp = X + 0.3f * rad, hyp = Y + 0.4f * rad;
            float t = fsq(hxp * hxp + hyp * hyp) / (rad * 1.4f);
            u32 c0 = hi, c1 = mid;
            if (t >= 0.45f) { c0 = mid; c1 = lo; t = (t - 0.45f) / 0.55f; }
            else t = t / 0.45f;
            if (t > 1) t = 1;
            blend(i, j, lerp8(CR(c0), CR(c1), t), lerp8(CG(c0), CG(c1), t), lerp8(CB(c0), CB(c1), t), (int)((float)a * co));
        }
}

/* A small right-pointing triangle, 8 wide and 10 high, tip to the right. */
static void arrow(float x, float y, u32 c)
{
    int S = g_S;
    float inv = 1.0f / (float)S;
    for (int j = (int)((y - 1) * S); j < (int)((y + 11) * S); j++)
        for (int i = (int)((x - 1) * S); i < (int)((x + 9) * S); i++) {
            float X = ((float)i + 0.5f) * inv - x, Y = ((float)j + 0.5f) * inv - y - 5.0f;
            if (Y < 0) Y = -Y;
            float hh = 5.0f * (1.0f - X / 8.0f), co = 0.5f + (hh - Y) * (float)S * 0.8f;
            float cx = 0.5f + X * (float)S, cr = 0.5f + (8.0f - X) * (float)S;
            if (cx < co) co = cx;
            if (cr < co) co = cr;
            if (co <= 0) continue;
            if (co > 1) co = 1;
            blend(i, j, CR(c), CG(c), CB(c), (int)(255.0f * co));
        }
}

/* ---- text ---------------------------------------------------------------- */

/* Faces: 0 body, 1 small, 2 title (small caps), 3 tiny. Glyph cells are 8-bit alpha at twice the
 * logical size. A code point maps to a cell: ASCII 32..126, Latin-1 160..255, then the angle quotes. */
static int next_glyph(const char **ps)
{
    const u8 *p = (const u8 *)*ps;
    unsigned c = *p++;
    if (c >= 0xC2 && c < 0xE0 && (*p & 0xC0) == 0x80) { c = ((c & 0x1Fu) << 6) | (*p & 0x3Fu); p++; }
    else if (c == 0xE2 && p[0] == 0x80 && (p[1] == 0xB9 || p[1] == 0xBA)) { c = p[1] == 0xB9 ? 0x2039u : 0x203Au; p += 2; }
    else if (c >= 0x80) c = '?';
    *ps = (const char *)p;
    if (c >= 32 && c < 127) return (int)c - 32;
    if (c >= 160 && c < 256) return 95 + (int)c - 160;
    if (c == 0x2039) return 191;
    if (c == 0x203A) return 192;
    return '?' - 32;
}

static int g_dx2;     /* extra shift of the first letter, in 2x pixels */
static int g_track;   /* extra space after every letter, in 2x pixels (the title is spaced out) */

static int text_w2(int f, const char *s)
{
    int w = 0;
    while (*s) w += wf_adv[f * WF_FONT_GLYPHS + next_glyph(&s)] + g_track;
    return w;
}

static int text_w(int f, const char *s) { return (text_w2(f, s) + 1) / 2; }

static int gv(const u8 *gl, int cw, int ch, int x, int y)
{
    if (x < 0 || y < 0 || x >= cw || y >= ch) return 0;
    return gl[y * cw + x];
}

/* Draws s with its left edge at logical lx and the middle of its capital letters on the line cy2 / 2. */
static void text(int f, int lx, int cy2, const char *s, u32 col, int a)
{
    int S = g_S;
    int cw = wf_face_w[f], ch = wf_face_h[f], pad = wf_face_pad[f], asc = wf_face_asc[f], cap = wf_face_cap[f];
    const u8 *base = wf_font_blob + wf_face_off[f];
    int ytop = (cy2 * S) / 2 + (cap * S) / 4 - (asc * S) / 2;
    int pen = g_dx2;
    while (*s) {
        int g = next_glyph(&s);
        const u8 *gl = base + (long)g * cw * ch;
        const u8 *bb = wf_bb + (f * WF_FONT_GLYPHS + g) * 4;
        if (bb[2] > bb[0]) {
            int xl = lx * S + ((pen - pad) * S) / 2;
            int i0 = ((bb[0] - 1) * S) / 2 - 1, i1 = ((bb[2] + 1) * S) / 2 + 2;
            int j0 = ((bb[1] - 1) * S) / 2 - 1, j1 = ((bb[3] + 1) * S) / 2 + 2;
            if (i0 < 0) i0 = 0;
            if (j0 < 0) j0 = 0;
            for (int y = j0; y < j1; y++) {
                int sy = ((2 * y + 1) * 256) / S - 128;
                int yy = sy >> 8, fy = sy & 255;
                for (int x = i0; x < i1; x++) {
                    int sx = ((2 * x + 1) * 256) / S - 128;
                    int xi = sx >> 8, fx = sx & 255;
                    int cov = (gv(gl, cw, ch, xi, yy) * (256 - fx) * (256 - fy) + gv(gl, cw, ch, xi + 1, yy) * fx * (256 - fy) +
                               gv(gl, cw, ch, xi, yy + 1) * (256 - fx) * fy + gv(gl, cw, ch, xi + 1, yy + 1) * fx * fy) >> 16;
                    if (cov) blend(xl + x, ytop + y, CR(col), CG(col), CB(col), a * cov / 255);
                }
            }
        }
        pen += wf_adv[f * WF_FONT_GLYPHS + g] + g_track;
    }
}

static void text_c(int f, int cx, int cy2, const char *s, u32 col)
{
    text(f, cx - text_w(f, s) / 2, cy2, s, col, 255);
}

static void text_r(int f, int rx, int cy2, const char *s, u32 col)
{
    text(f, rx - text_w(f, s), cy2, s, col, 255);
}

/* ---- icons --------------------------------------------------------------- */

/* Fallbacks so a checkout without art still builds (icon 0 is drawn when art exists). */
#ifndef ICON_TAB_PLAYER
#define ICON_TAB_PLAYER 0
#define ICON_ROW_GOD 0
#define ICON_ROW_RUPEES 0
#define ICON_ROW_ITEMS 0
#define ICON_ROW_MAGIC 0
#define ICON_ROW_TIME 0
#define ICON_ROW_MARK 0
#define ICON_ROW_RETURN 0
#define ICON_ROW_AGE 0
#define ICON_ROW_SPEED 0
#define ICON_ROW_JUMP 0
#define ICON_ROW_CHICKEN 0
#define ICON_ROW_STAR 0
#define ICON_SWORD_KOKIRI 0
#endif
#ifndef ICON_COUNT
#define ICON_COUNT 0
#define ICON_PX 64
#endif

static int g_icon_am = 255;   /* alpha of the next icons, 0..255 */

static int px_a(const u8 *src, int x, int y, int c)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= ICON_PX) x = ICON_PX - 1;
    if (y >= ICON_PX) y = ICON_PX - 1;
    return src[(y * ICON_PX + x) * 4 + c];
}

/* Draws icon idx into a sz x sz logical square: box filter when shrinking, bilinear when growing.
 * Colours are weighted by alpha so transparent pixels do not leak their colour into the edges. */
static void draw_icon(int lx, int ly, int sz, int idx)
{
    if (idx < 0 || idx >= ICON_COUNT) return;
    const u8 *src = wf_icon_blob + (long)idx * ICON_PX * ICON_PX * 4;
    int d = sz * g_S;
    for (int y = 0; y < d; y++) {
        for (int x = 0; x < d; x++) {
            long sa = 0, sr = 0, sg = 0, sb = 0, wsum = 0;
            if (d < ICON_PX) {
                int x0 = x * ICON_PX / d, x1 = (x + 1) * ICON_PX / d;
                int y0 = y * ICON_PX / d, y1 = (y + 1) * ICON_PX / d;
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                for (int yy = y0; yy < y1; yy++)
                    for (int xx = x0; xx < x1; xx++) {
                        int a = px_a(src, xx, yy, 3);
                        sa += a; wsum++;
                        sr += px_a(src, xx, yy, 0) * a;
                        sg += px_a(src, xx, yy, 1) * a;
                        sb += px_a(src, xx, yy, 2) * a;
                    }
                if (!sa) continue;
                blend(lx * g_S + x, ly * g_S + y, (int)(sr / sa), (int)(sg / sa), (int)(sb / sa), (int)(sa / wsum) * g_icon_am / 255);
            } else {
                int sx = ((2 * x + 1) * ICON_PX * 128) / d - 128;
                int sy = ((2 * y + 1) * ICON_PX * 128) / d - 128;
                int xi = sx >> 8, fx = sx & 255, yi = sy >> 8, fy = sy & 255;
                for (int k = 0; k < 4; k++) {
                    int ox = xi + (k & 1), oy = yi + (k >> 1);
                    int w = ((k & 1) ? fx : 256 - fx) * ((k >> 1) ? fy : 256 - fy) >> 8;
                    int a = px_a(src, ox, oy, 3) * w;
                    sa += a; wsum += w;
                    sr += px_a(src, ox, oy, 0) * (long)a;
                    sg += px_a(src, ox, oy, 1) * (long)a;
                    sb += px_a(src, ox, oy, 2) * (long)a;
                }
                if (!sa) continue;
                blend(lx * g_S + x, ly * g_S + y, (int)(sr / sa), (int)(sg / sa), (int)(sb / sa), (int)(sa / (wsum ? wsum : 1)) * g_icon_am / 255);
            }
        }
    }
}

/* The owner's logo, drawn into a box lw x lh logical (its blob is LOGO_W x LOGO_H); area average. */
static void draw_logo(int lx, int ly, int lh)
{
#if LOGO_W > 0
    int dh = lh * g_S, dw = dh * LOGO_W / LOGO_H;
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            int x0 = x * LOGO_W / dw, x1 = (x + 1) * LOGO_W / dw, y0 = y * LOGO_H / dh, y1 = (y + 1) * LOGO_H / dh;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            long sa = 0, sr = 0, sg = 0, sb = 0, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const u8 *q = wf_logo_blob + ((long)yy * LOGO_W + xx) * 4;
                    sa += q[3]; n++;
                    sr += q[0] * (long)q[3]; sg += q[1] * (long)q[3]; sb += q[2] * (long)q[3];
                }
            if (!sa) continue;
            blend(lx * g_S + x, ly * g_S + y, (int)(sr / sa), (int)(sg / sa), (int)(sb / sa), (int)(sa / n));
        }
#else
    (void)lx; (void)ly; (void)lh;
#endif
}

/* ---- widgets ------------------------------------------------------------- */

#define AMB0 0xf9c35a   /* glossy amber, light end */
#define AMB1 0xd9801c   /* glossy amber, dark end */
#define EDGE 0x3a2412   /* dark brown outline */

/* A glossy amber plate with a dark outline and a bright band across the top half. */
static void plate(float x, float y, float w, float h, float rad)
{
    rr(x, y + 0.8f, w, h, rad, 0, 0, 0, 70);
    rr(x, y, w, h, rad, 0, AMB0, AMB1, 255);
    rr(x + 1.5f, y + 1.2f, w - 3.0f, h * 0.42f, rad * 0.6f, 0, 0xffffff, 0xffe9a8, 90);
    rr(x, y, w, h, rad, 1.4f, EDGE, EDGE, 255);
}

/* The highlight on the selected row. */
static void sel_box(float x, float y, float w, float h)
{
    rr(x, y, w, h, 4, 0, 0xf0b862, 0xd08a3a, 85);          /* warm glow, no heavy ring that could touch the text */
    rr(x, y, w, h, 4, 1.0f, 0x9a6226, 0x9a6226, 150);
    rr(x + 3.0f, y + h * 0.5f - 3.0f, 2.0f, 6.0f, 1.0f, 0, 0x8a5020, 0x8a5020, 220);   /* small marker at the left */
}

/* A wooden toggle, 40 x 16, like a carved wooden slider: dark rim, plank inside, round wooden knob. */
static void slider(int lx, float ly, int on)
{
    float fx = (float)lx, fy = ly;
    rr(fx, fy + 0.8f, 40, 16, 8, 0, 0, 0, 70);                                          /* drop shadow */
    rr(fx, fy, 40, 16, 8, 0, 0x6b3d1c, 0x3a2010, 255);                                  /* rim */
    rr(fx + 1.5f, fy + 1.5f, 37, 13, 6.5f, 0, on ? 0xf2b25e : 0x9a7048, on ? 0xd98a3a : 0x6e4a2c, 255);   /* plank */
    static const float gy[3] = {5.0f, 8.0f, 11.0f};                                     /* wood grain */
    static const float gx[3] = {9.0f, 15.0f, 8.0f};
    static const float gw[3] = {20.0f, 16.0f, 23.0f};
    for (int k = 0; k < 3; k++) rr(fx + gx[k], fy + gy[k], gw[k], 0.7f, 0.35f, 0, 0x5a2e10, 0x5a2e10, on ? 70 : 90);
    rr(fx + 3.5f, fy + 2.2f, 33, 3.5f, 1.7f, 0, 0xffffff, 0xffe0a0, on ? 80 : 40);      /* sheen */
    rr(fx + 1.5f, fy + 1.5f, 37, 13, 6.5f, 1.2f, 0x4a2a14, 0x4a2a14, 150);              /* inner shadow */
    float kx = fx + (on ? 31.0f : 9.0f), ky = fy + 8.0f;
    disc(kx, ky + 1.0f, 7.0f, 0, 0, 0, 90);
    disc(kx, ky, 7.0f, 0x8a5224, 0x5a3014, 0x3a2010, 255);                              /* knob rim */
    disc(kx, ky, 5.7f, 0xffd98f, 0xeaa04a, 0xb8702a, 255);                              /* knob */
    rr(kx - 3.0f, ky - 4.0f, 5.0f, 1.5f, 0.75f, 0, 0xffffff, 0xffffff, 110);            /* knob glint */
}

/* A carved wooden plate with a round gem: green = yes, red = no. 22 x 22, top-left at (x, y). */
static void gem_btn(float x, float y, int green)
{
    float cx = x + 11.0f, cy = y + 11.0f;
    rr(x, y + 0.8f, 22, 22, 7, 0, 0, 0, 70);
    rr(x, y, 22, 22, 7, 0, 0x6b3d1c, 0x3a2010, 255);
    rr(x + 1.5f, y + 1.5f, 19, 19, 5.5f, 0, 0xf2b25e, 0xd98a3a, 255);
    rr(x + 1.5f, y + 1.5f, 19, 19, 5.5f, 1.2f, 0x4a2a14, 0x4a2a14, 150);
    disc(cx, cy + 0.6f, 7.4f, 0, 0, 0, 90);
    disc(cx, cy, 7.4f, 0x8a5224, 0x5a3014, 0x3a2010, 255);
    if (green) disc(cx, cy, 6.0f, 0xc8ffb0, 0x4fcf3f, 0x1f8a2a, 255);
    else disc(cx, cy, 6.0f, 0xffb0a0, 0xe0382c, 0x8a1810, 255);
    rr(cx - 3.5f, cy - 4.2f, 5.0f, 2.2f, 1.1f, 0, 0xffffff, 0xffffff, 150);
}

/* Signed distance from (px, py) to a triangle (negative inside); the usual exact formula. */
static float sd_tri(float px, float py, const float *t)
{
    float e[3][2], v[3][2], dd = 1e30f, ds = 1e30f;
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        e[i][0] = t[2 * j] - t[2 * i]; e[i][1] = t[2 * j + 1] - t[2 * i + 1];
        v[i][0] = px - t[2 * i]; v[i][1] = py - t[2 * i + 1];
    }
    float sg = e[0][0] * e[2][1] - e[0][1] * e[2][0];
    sg = sg < 0 ? -1.0f : 1.0f;
    for (int i = 0; i < 3; i++) {
        float k = (v[i][0] * e[i][0] + v[i][1] * e[i][1]) / (e[i][0] * e[i][0] + e[i][1] * e[i][1]);
        k = k < 0 ? 0 : k > 1 ? 1 : k;
        float qx = v[i][0] - e[i][0] * k, qy = v[i][1] - e[i][1] * k;
        float q2 = qx * qx + qy * qy, c = sg * (v[i][0] * e[i][1] - v[i][1] * e[i][0]);
        if (q2 < dd) dd = q2;
        if (c < ds) ds = c;
    }
    float d = fsq(dd);
    return ds > 0 ? -d : d;
}

/* A rounded triangle pointing left (dir -1) or right (dir 1), w wide and h high, centred at (cx, cy).
 * shrink pulls the edge in (logical units); the fill runs from c0 at the top to c1 at the bottom. */
static void tri(float cx, float cy, float w, float h, int dir, float shrink, u32 c0, u32 c1, int a)
{
    float L = fsq(w * w + h * h * 0.25f), area = w * h * 0.5f, rho = area / (L + h * 0.5f);
    float k = 0.62f, r = (1.0f - k) * rho + 0.0f;
    float vx[3] = {-w * 0.5f * dir, w * 0.5f * dir, -w * 0.5f * dir}, vy[3] = {-h * 0.5f, 0.0f, h * 0.5f};
    float gx = (vx[0] + vx[1] + vx[2]) / 3.0f, t[6];
    for (int i = 0; i < 3; i++) { t[2 * i] = cx + gx + k * (vx[i] - gx); t[2 * i + 1] = cy + k * vy[i]; }
    int S = g_S;
    float inv = 1.0f / (float)S;
    for (int j = (int)((cy - h * 0.5f - 2) * S); j < (int)((cy + h * 0.5f + 2) * S); j++) {
        float Y = ((float)j + 0.5f) * inv, tt = (Y - (cy - h * 0.5f)) / h;
        tt = tt < 0 ? 0 : tt > 1 ? 1 : tt;
        int rr_ = lerp8(CR(c0), CR(c1), tt), gg = lerp8(CG(c0), CG(c1), tt), bb = lerp8(CB(c0), CB(c1), tt);
        for (int i = (int)((cx - w * 0.5f - 2) * S); i < (int)((cx + w * 0.5f + 2) * S); i++) {
            float X = ((float)i + 0.5f) * inv;
            float sd = sd_tri(X, Y, t) - r + shrink, co = 0.5f - sd * (float)S;
            if (co <= 0) continue;
            if (co > 1) co = 1;
            blend(i, j, rr_, gg, bb, (int)((float)a * co));
        }
    }
}

/* One wooden arrow button, like the amber carved triangles of the reference. */
static void arrow_btn(float cx, float cy, int dir)
{
    tri(cx, cy + 0.8f, 13, 16, dir, 0, 0, 0, 70);
    tri(cx, cy, 13, 16, dir, 0, 0x6b3d1c, 0x3a2010, 255);
    tri(cx, cy, 13, 16, dir, 1.2f, AMB0, AMB1, 255);
    tri(cx - 0.3f * (float)dir, cy - 1.0f, 9, 8, dir, 0.6f, 0xffffff, 0xffe0a0, 80);
}

/* "(<) [ 2x ] (>)": two wooden arrows around a small dark plank with the value, centred on logical cx
 * (the same centre as the toggles above it). */
static void selector(int cx, int cy2, const char *val)
{
    float cy = (float)cy2 * 0.5f;
    int pw = text_w(4, val) + 12;
    if (pw < 34) pw = 34;
    float x0 = (float)cx - (float)(pw + 30) * 0.5f, px = x0 + 15.0f;
    arrow_btn(x0 + 6.5f, cy, -1);
    rr(px, cy - 7.5f, (float)pw, 15, 4, 0, 0x24170c, 0x4a3320, 255);
    rr(px, cy - 7.5f, (float)pw, 15, 4, 1.2f, EDGE, EDGE, 255);
    text_c(4, (int)(px + (float)pw * 0.5f), cy2, val, CREAMC);
    arrow_btn(px + (float)pw + 2.0f + 6.5f, cy, 1);
}

/* One hint in the footer: a round button glyph per key, then a label. Returns where the next hint starts. */
static int hint(int x, const char *keys, const char *label)
{
    for (; *keys; keys++) {
        char s[2] = {*keys, 0};
        plate((float)x, 282.0f, 15, 15, 7.5f);
        {   /* centre the ink of the letter (not its advance width) in the round plate */
            const char *q = s;
            int g = next_glyph(&q);
            const u8 *bb = wf_bb + (3 * WF_FONT_GLYPHS + g) * 4;
            g_dx2 = 15 - ((bb[0] + bb[2]) / 2 - wf_face_pad[3]);
            text(3, x, 579, s, EDGE, 255);
            g_dx2 = 0;
        }
        x += 19;
    }
    text(1, x, 579, label, FOOTC, 255);
    return x + text_w(1, label) + 16;
}

/* ---- the panel ----------------------------------------------------------- */

static void paint_panel(void)
{
    paint_bg();

    g_track = 4;
#if LOGO_W > 0
    draw_logo(46, 38, 20);
#else
    text(2, 48, 95, "WAYFARER", WOOD, 255);
#endif
    g_track = 0;
#if LOGO_W > 0
    text(1, 46 + 20 * LOGO_W / LOGO_H + 7, 98, "by melianluthien", 0x8a7440, 255);
#endif
    text_r(3, 492, 96, WF_VERSION, 0x7a6234);

    for (int t = 0; t < TABS; t++) {
        float x = 46.0f + (float)t * 113.5f;
        if (t == g_tab) {
            plate(x, 64, 107.5f, 28, 14);
        } else {
            rr(x, 64, 107.5f, 28, 14, 0, WOOD, WOOD, 41);
        }
        draw_icon((int)(x + 4.0f), 68, 20, ICON_TAB_PLAYER + t);
        text(0, (int)(x + 32.0f), 156, nth(tabs_blob, t), t == g_tab ? EDGE : DESC, 255);
    }
    fill(46, 98, 448, 1, BRASS, 204);

    if (g_tab == 0) {
        static const u8 row_icon[PLAYER_ROWS] = {ICON_ROW_GOD, ICON_ROW_MAGIC, ICON_ROW_RUPEES, ICON_ROW_ITEMS, ICON_ROW_CHICKEN, ICON_ROW_FLY, ICON_ROW_SPEED, ICON_ROW_JUMP, ICON_ROW_AGE};
        for (int i = 0; i < PLAYER_ROWS; i++) {
            int y = 102 + i * 17;
            if (i == g_sel) sel_box(46, (float)y, 448, 17);
            draw_icon(52, y + 1, 15, row_icon[i]);
            text(0, 79, 2 * y + 17, row_label(i), INK, 255);
            if (i < 6) slider(436, (float)y + 0.5f, g_cheat[player_what[i]] ? 1 : 0);
            else if (i < 8) {
                static const char sp_txt[] = "1x\0" "1.5x\0" "2x\0" "3x";
                static const char jp_txt[] = "1x\0" "2x\0" "3x\0" "5x";
                selector(456, 2 * y + 17, nth(i == 6 ? sp_txt : jp_txt, (i == 6 ? g_speed : g_jump) & 3));
            }
        }
        text(1, 48, 532, row_desc(g_sel), DESC, 255);
    } else if (g_tab == 1) {
        int top = g_isel / 12 - 1, rows = (ITEM_COUNT + 11) / 12;
        if (top > rows - 4) top = rows - 4;
        if (top < 0) top = 0;
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 12; c++) {
                int i = (top + r) * 12 + c;
                if (i >= ITEM_COUNT) break;
                int x = 46 + (c * 412 + 5) / 11, y = 104 + r * 37, have = g_icount[i] > 0;
                float fx = (float)x, fy = (float)y;
                rr(fx, fy + 1, 36, 36, 3, 0, 0xfff4d6, 0xfff4d6, 150);          /* light edge under the slot */
                if (i == g_isel) {
                    rr(fx, fy, 36, 36, 3, 0, GOLDC, 0xd9bd6a, 140);
                    rr(fx - 1, fy - 1, 38, 38, 4, 1, GOLDC, GOLDC, 255);
                    rr(fx, fy, 36, 36, 3, 2.5f, 0x3f2e1c, 0x3f2e1c, 255);
                } else {
                    rr(fx, fy, 36, 36, 3, 0, WOOD, WOOD, 52);
                    rr(fx, fy, 36, 36, 3, 1.5f, 0x7a6234, 0x7a6234, 255);
                    rr(fx + 1.5f, fy + 1.5f, 33, 3, 1.5f, 0, 0, 0, 36);           /* inner shadow at the top */
                }
                int ic = i < 42 ? ICON_SWORD_KOKIRI + i : i < 56 ? ICON_SWORD_KOKIRI + i :
                         i < 58 ? ICON_WALLET_ADULT + (i - 56) : i < 61 ? ICON_MAGIC_METER + (i - 58) :
                         i < 70 ? ICON_STONE_KOKIRI + (i - 61) : i < 82 ? ICON_SONG_ZELDA + (int)"\0\4\5\2\1\3\6\7\10\12\11\13"[i - 70] :
                         ICON_STONE_AGONY + (i - 82);
                g_icon_am = have ? 255 : 82;
                draw_icon(x + 3, y + 3, 30, ic);
                g_icon_am = 255;
            }
        {
            char st[16];
            int n = g_icount[g_isel], k = 0;
            if (ITEM_IS_QTY(g_isel)) {
                if (n > 0) {
                    char d[8];
                    int m = 0;
                    st[k++] = 'x';
                    st[k++] = ' ';
                    while (n > 0 && m < 7) { d[m++] = (char)('0' + n % 10); n /= 10; }
                    while (m > 0) st[k++] = d[--m];
                    st[k] = 0;
                } else { st[0] = '-'; st[1] = 0; }
            } else {
                const char *w = n > 0 ? "Owned" : "-";
                while (*w) st[k++] = *w++;
                st[k] = 0;
            }
            {
                static const char need_txt[] = "the Deku-Stick Bag\0the Deku-Nut Bag\0the Bomb Bag\0the Arrow Quiver\0the Deku-Seed Bag\0";
                int need = g_isel == 11 ? 0 : g_isel == 12 ? 1 : g_isel == 13 ? 2 : g_isel == 18 ? 3 : g_isel == 16 ? 4 : -1;
                char ln[112];
                int q = 0;
                for (const char *w = nth(item_labels, g_isel); *w && q < 60; ) ln[q++] = *w++;
                if (need >= 0) {
                    for (const char *w = " - needs "; *w; ) ln[q++] = *w++;
                    for (const char *w = nth(need_txt, need); *w && q < 110; ) ln[q++] = *w++;
                }
                ln[q] = 0;
                text(1, 48, 532, ln, DESC, 255);
            }
            text_r(1, 492, 532, st, g_icount[g_isel] > 0 ? GREEN : DESC);
        }
    } else if (g_tab == 3) {
        int top = g_tsel - 3;
        if (top > TP_COUNT - 7) top = TP_COUNT - 7;
        if (top < 0) top = 0;
        if (!g_tl_ok) tp_rebuild();
        for (int k = 0; k < 7; k++) {
            int i = top + k, y = 104 + k * 21;
            if (i == g_tsel) { sel_box(46, (float)y, 436, 21); arrow(58, (float)y + 6, WOOD); }
            text(0, 80, 2 * y + 21, nth(tp_names, g_tlist[i]), INK, 255);
            if ((g_tfav >> g_tlist[i]) & 1) draw_icon(458, y + 1, 19, ICON_ROW_STAR);
        }
        rr(490, 104, 4, 147, 2, 0, WOOD, WOOD, 51);
        rr(490, 104.0f + (float)(147 - 36) * (float)g_tsel / (float)(TP_COUNT - 1), 4, 36, 2, 0, WOOD, WOOD, 255);
        text(1, 48, 532, g_tsort ? "Sorted by order of discovery" : "Sorted alphabetically", DESC, 255);
    } else if (g_tab == 2) {
        static const u8 wicon[WORLD_ROWS] = {ICON_ROW_MARK, ICON_ROW_RETURN, ICON_ROW_TIME, ICON_ROW_CAMERA};
        for (int i = 0; i < WORLD_ROWS; i++) {
            int y = 104 + i * 28;
            if (i == g_wsel) sel_box(46, (float)y, 448, 26);
            draw_icon(52, y + 3, 19, wicon[i]);
            text(0, 79, 2 * y + 26, nth(wlabels_blob, i), INK, 255);
            if (i == 2) {
                char t[8];
                t[0] = (char)('0' + g_hour / 10);
                t[1] = (char)('0' + g_hour % 10);
                t[2] = ':'; t[3] = '0'; t[4] = '0'; t[5] = 0;
                selector(456, 2 * y + 26, t);
            } else if (i < 2) {
                gem_btn(470, (float)y + 2, g_marked);
            } else if (i == 3) {
                slider(436, (float)y + 5, g_cheat[5] ? 1 : 0);
            }
        }
        text(1, 48, 532, nth(wdescs_blob, g_wsel), DESC, 255);
    }

    fill(46, 277, 448, 1, BRASS, 204);
    {
        int x = 48;
        x = hint(x, "A", g_tab == 3 ? "Go" : g_tab == 1 ? "Toggle" : "Select");
        if (g_tab == 1 && ITEM_IS_QTY(g_isel)) { x = hint(x, "X", "Increase"); x = hint(x, "Y", "Decrease"); }
        if (g_tab == 3) { x = hint(x, "X", "Favorite"); x = hint(x, "Y", "Sort"); }
        x = hint(x, "B", "Close");
        (void)x;
        hint(492 - (38 + text_w(1, "Tab")), "LR", "Tab");
    }

    if (g_dlg) {
        rr(78, 96, 384, 148, 8, 0, 0x3f2e1c, 0x2a1d12, 255);
        rr(78, 96, 384, 148, 8, 2, GOLDC, GOLDC, 255);
        text_c(0, 270, 240, "Cheats are experimental", GOLDC);
        text_c(1, 270, 296, "They can crash the game or damage", CREAMC);
        text_c(1, 270, 326, "your save. Use a copy of your save.", CREAMC);
        text(0, 150, 392, "A   Accept for this session", 0x96e278, 255);
        text(0, 150, 428, "B   Cancel", 0xd6bea8, 255);
    }
}
