/* No-clip flight, movement speed and free camera. Included by cheats.h after cheats2.h.
 * Flight and the camera follow Hookshot's cheats (GPL-3.0); speed scales Link's own per-frame movement. */

#define OFF_CAMMGR      0x4c86bc0ull   /* -> holder -> camera manager */
#define VIEW_VT         0x4bf8a88ull   /* vtable of a camera's view */

/* ---- small float maths (the module has no libm) ---- */
#define PI_F 3.14159265f

static float fsqrt_(float x)
{
    float r;
    __asm__("fsqrt %s0, %s1" : "=w"(r) : "w"(x));
    return r;
}
static float fabs_(float x) { return x < 0 ? -x : x; }

static float fsin_(float x)
{
    while (x > PI_F) x -= 2 * PI_F;
    while (x < -PI_F) x += 2 * PI_F;
    if (x > PI_F / 2) x = PI_F - x;
    else if (x < -PI_F / 2) x = -PI_F - x;
    float x2 = x * x;
    return x * (1.f - x2 / 6.f * (1.f - x2 / 20.f * (1.f - x2 / 42.f * (1.f - x2 / 72.f))));
}
static float fcos_(float x) { return fsin_(x + PI_F / 2); }

static float fatan_(float z)   /* |z| <= 1 */
{
    float z2 = z * z;
    return z * (0.9998660f + z2 * (-0.3302995f + z2 * (0.1801410f + z2 * (-0.0851330f + z2 * 0.0208351f))));
}

static float fatan2_(float y, float x)
{
    float ax = fabs_(x), ay = fabs_(y), r;
    if (ax == 0.f && ay == 0.f) return 0.f;
    if (ax >= ay) r = fatan_(ay / ax);
    else r = PI_F / 2 - fatan_(ax / ay);
    if (x < 0) r = PI_F - r;
    return y < 0 ? -r : r;
}

static float f_at(u64 addr) { return *(volatile float *)addr; }

/* Stick value (-32767..32767) as -1..1 with a small dead zone. */
static float stick(int v)
{
    float f = (float)v / 32767.f;
    if (f > -0.15f && f < 0.15f) return 0.f;
    return f > 1.f ? 1.f : f < -1.f ? -1.f : f;
}

static int ev_playing(void)
{
    u64 b = g_base, eh = *(u64 *)(b + OFF_EV_HOLDER);
    void *em = eh ? *(void **)eh : 0;
    return !em || ((FnEvent)(b + FN_EVENT))(em, 0);
}

/* The gameplay camera's view object (camera 0 of the camera manager). */
static u64 gameplay_view(void)
{
    u64 h = *(u64 *)(g_base + OFF_CAMMGR);
    u64 m = h ? *(u64 *)h : 0;
    if (!m || !*(u32 *)(m + 32)) return 0;
    u64 cams = *(u64 *)(m + 40);
    u64 cam = cams ? *(u64 *)cams : 0;
    return cam ? *(u64 *)(cam + 120) : 0;
}

/* ---- movement speed: the extra distance Link just walked, scaled ---- */

static const float speed_mul[4] = {1.0f, 1.5f, 2.0f, 3.0f};
static const float jump_mul[4] = {1.0f, 2.0f, 3.0f, 5.0f};
static u64 g_sp_actor;
static int g_clamp_log;
static float g_sp_prev[3], g_sp_pdy;

static void speed_step(u64 a)
{
    u64 c = link_controller(a);
    if (!c || ev_playing()) { g_sp_actor = 0; return; }
    float x = f_at(a + 796), y = f_at(a + 800), z = f_at(a + 804);
    if (g_sp_actor == a) {
        float dx = x - g_sp_prev[0], dy = y - g_sp_prev[1], dz = z - g_sp_prev[2], d2 = dx * dx + dz * dz;
        /* Rising fast, or rising while the rise slows down frame by frame (gravity): a jump, not a slope.
         * This holds while running too, where the sideways step is bigger than the rise. */
        int jumping = g_jump && dy > 0.03f && (dy * dy * 4.f > d2 || dy < g_sp_pdy - 0.002f);
        int moving = d2 > 4e-4f;   /* below this it is idle sway or floor snapping: scaling that makes Link fidget */
        /* chicken gravity: falling (not walking downhill) is slowed to a float, like carrying a cucco */
        int falling = g_cheat[6] && dy < -0.03f && (dy * dy > d2 || dy < g_sp_pdy - 0.002f);
        {   /* probe: with Chicken Gravity OFF, dump Link's actor and controller on the 25th frame of a fall (a normal fall, then a fall holding a cucco) */
            static int air, dumps;
            int dn = !g_cheat[6] && dy < -0.03f && (dy * dy > d2 || dy < g_sp_pdy - 0.002f);
            air = dn ? air + 1 : 0;
            if (air == 25 && dumps < 4) {
                dbgx("DUMP number", (u64)dumps);
                for (int who = 0; who < 2; who++)
                    for (int i = 0; i < 512; i++) {
                        u32 w = *(u32 *)((who ? c : a) + i * 4);
                        if (w) { dbgx("dump off", ((u64)who << 16) | (u64)(i * 4)); dbgx("dump word", w); }
                    }
                dumps++;
            }
        }
        g_sp_pdy = dy;
        /* The game's own downward speed (controller +0x2d4, in units per second) keeps growing the longer you fall;
         * past a limit it plays the freefall animation and hurts. Holding it down keeps a fall a gentle float. */
        if (falling && !jumping) {
            float *vy = (float *)(c + 0x2d4), *ay = (float *)(a + 0x38c), *by = (float *)(a + 0x3a4);   /* the actor keeps copies */
            if (g_clamp_log == 20 || g_clamp_log == 21) {   /* find the air-time timer: in objects Link points to, words that go up by 1, or floats that creep up by one frame's worth */
                static u64 sp[48];
                static u32 sw[48][0x180];
                static int np;
                if (g_clamp_log == 20) {
                    np = 0;
                    for (int who = 0; who < 2; who++)
                        for (int off = 0; off < 0x800 && np < 48; off += 8) {
                            u64 p = *(u64 *)((who ? c : a) + off);
                            long long dd = (long long)p - (long long)a;
                            if ((p & 7) || dd > 0x4000000LL || dd < -0x4000000LL || (p >> 12) == (a >> 12) || (p >> 12) == (c >> 12)) continue;
                            int dup = 0;
                            for (int k = 0; k < np; k++) if (sp[k] == p) dup = 1;
                            if (dup) continue;
                            sp[np] = p;
                            for (int i = 0; i < 0x180; i++) sw[np][i] = *(u32 *)(p + i * 4);
                            np++;
                        }
                } else {
                    for (int k = 0; k < np; k++)
                        for (int i = 0; i < 0x180; i++) {
                            u32 o = sw[k][i], n = *(u32 *)(sp[k] + i * 4);
                            float fo = *(float *)&o, fn = *(float *)&n;
                            int hit = (n == o + 1 && o > 0 && o < 0x1000000) || (fn > fo + 0.004f && fn < fo + 0.12f && fn > 0.02f && fn < 100.0f && fo > 0.0f);
                            if (hit) { dbgx("timer? ptr k", (u64)k); dbgx("timer? word offset", (u64)i * 4); dbgx("timer? old|new", ((u64)o << 32) | n); }
                        }
                    dbgx("timer scan done, objects", (u64)np);
                }
            }
            {   /* the game decides the freefall animation and damage by fall HEIGHT: look for the stored height the fall started from */
                static float y0;
                if (g_clamp_log == 0) y0 = g_sp_prev[1];
                if (g_clamp_log == 30) {
                    float yn = g_sp_prev[1];
                    dbgx("fall start y bits", *(u32 *)&y0);
                    dbgx("fall now y bits", *(u32 *)&yn);
                    for (int who = 0; who < 3; who++) {
                        u64 base = who == 0 ? a : who == 1 ? c : *(u64 *)(c + 0x20);
                        if (!base) continue;
                        for (int i = 0; i < 0x200; i++) {
                            float v = *(float *)(base + i * 4);
                            if (v > yn + 1.0f && v < y0 + 0.5f) {
                                dbgx("height? who(0 actor 1 ctl 2 obj20)<<16|off", ((u64)who << 16) | (u64)(i * 4));
                                dbgx("height? value bits", *(u32 *)&v);
                            }
                        }
                    }
                }
            }
            if (g_clamp_log >= 1) *(float *)(a + 0x2f0) = g_sp_prev[1];   /* test: a stored height found between the start and the current height of a fall */
            if (g_clamp_log >= 30 && g_clamp_log < 34) dbgx("actor 2f0 now", *(u32 *)(a + 0x2f0));
            {   /* state machine probe: log every small-integer word of the actor and controller that changes during a fall, with the frame number */
                static u32 snap[2][1024];
                static int logged;
                if (g_clamp_log == 0) { logged = 0; for (int w = 0; w < 2; w++) for (int i = 0; i < 1024; i++) snap[w][i] = *(u32 *)((w ? c : a) + i * 4); }
                else {
                    for (int w = 0; w < 2; w++)
                        for (int i = 0; i < 1024; i++) {
                            u32 n = *(u32 *)((w ? c : a) + i * 4), o = snap[w][i];
                            snap[w][i] = n;
                            if (n == o || n > 0x10000 || o > 0x10000 || (w == 0 && i * 4 == 0x6ac) || logged > 400) continue;
                            logged++;
                            dbgx("st frame", (u64)g_clamp_log);
                            dbgx("st off", ((u64)w << 16) | (u64)(i * 4));
                            dbgx("st old|new", ((u64)o << 32) | n);
                        }
                }
            }
            /* test: two controller flags that switch on about three seconds into a fall, when the freefall starts */
            *(u32 *)(c + 0x220) = 0;
            *(u32 *)(c + 0x384) = 0;
            g_clamp_log++;
            if (*vy < -3.5f) *vy = -3.5f;
            if (*ay < -3.5f) *ay = -3.5f;
            if (*by < -3.5f) *by = -3.5f;
            { float *cp = (float *)(c + 0x6d8); if (*cp < -3.5f) *cp = -3.5f; }
            {   /* the object at controller +0x20 holds two more copies of the speed (found by the scan) */
                u64 o = *(u64 *)(c + 0x20);
                if (o && !(o & 7)) {
                    float *q1 = (float *)(o + 0xe4), *q2 = (float *)(o + 0x21c);
                    if (g_clamp_log >= 30 && g_clamp_log < 36) { dbgx("obj e4 bits", *(u32 *)q1); dbgx("obj 21c bits", *(u32 *)q2); }
                    if (*q1 < -3.5f) *q1 = -3.5f;
                    if (*q2 < -3.5f) *q2 = -3.5f;
                }
            }   /* the controller's own copy of the actor's speed */
        } else if (!falling) g_clamp_log = 0;
        if ((moving || jumping || falling) && (d2 + dy * dy < 1.0f || (falling && !jumping && d2 < 1.0f && dy > -8.0f))) {   /* ordinary walking; a bigger jump is a teleport or a cutscene */
            static float arg[4] __attribute__((aligned(16)));
            float m = moving ? speed_mul[g_speed & 3] : 1.0f, mv = m;
            if (jumping) mv = jump_mul[g_jump & 3];
            else if (falling) mv = 0.5f;   /* the game's fall speed keeps growing, so also cap the step: a float, never a freefall */
            x = g_sp_prev[0] + dx * m;
            z = g_sp_prev[2] + dz * m;
            y = g_sp_prev[1] + dy * mv;
            if (falling && !jumping && y < g_sp_prev[1] - 0.08f) y = g_sp_prev[1] - 0.08f;   /* the game's own fall speed keeps growing (0.9 per frame after a few seconds): hold a steady float */   /* scale the climb too, or slopes push Link under the floor */
            arg[0] = x; arg[1] = y; arg[2] = z; arg[3] = 0.f;
            ((void (*)(u64, float *))(g_base + FN_CHANGE_POS))(c, arg);
        }
    }
    g_sp_actor = a;
    g_sp_prev[0] = x; g_sp_prev[1] = y; g_sp_prev[2] = z;
}

/* ---- no-clip flight ---- */

static u64 g_fl_actor, g_fl_last;
static float g_fl_pos[3], g_fl_vel[3];

static void flight_step(u64 a)
{
    u64 c = link_controller(a);
    if (!c || ev_playing()) { g_fl_actor = 0; return; }
    u64 now = now_ms(), el = now - g_fl_last;
    if (g_fl_actor != a || el > 250) {
        for (int i = 0; i < 3; i++) { g_fl_pos[i] = f_at(a + 796 + i * 4); g_fl_vel[i] = 0.f; }
        g_fl_actor = a;
        g_fl_last = now;
        return;
    }
    if (!el) return;
    g_fl_last = now;
    float dt = (float)el / 1000.f;
    if (dt > 0.05f) dt = 0.05f;
    float right = stick(g_stk[0]), forward = stick(g_stk[1]);
    float vertical = (float)((int)((g_held >> 9) & 1) - (int)((g_held >> 8) & 1));   /* ZR up, ZL down */
    float rx = 1.f, rz = 0.f;
    u64 view = gameplay_view();
    if (view && *(u64 *)view == g_base + VIEW_VT) {
        float x = f_at(view + 8), z = f_at(view + 16), len = fsqrt_(x * x + z * z);
        if (len > 0.01f) { rx = x / len; rz = z / len; }
    }
    float d[3];
    d[0] = rx * right + rz * forward;
    d[1] = vertical;
    d[2] = rz * right - rx * forward;
    float len = fsqrt_(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    float speed = 8.f / (len > 1.f ? len : 1.f);
    float blend = 12.f * dt;
    if (blend > 1.f) blend = 1.f;
    for (int i = 0; i < 3; i++) {
        g_fl_vel[i] += (d[i] * speed - g_fl_vel[i]) * blend;
        g_fl_pos[i] += g_fl_vel[i] * dt;
    }
    static float arg[4] __attribute__((aligned(16)));
    arg[0] = g_fl_pos[0]; arg[1] = g_fl_pos[1]; arg[2] = g_fl_pos[2]; arg[3] = 0.f;
    ((void (*)(u64, float *))(g_base + FN_CHANGE_POS))(c, arg);
}

/* ---- free camera: called (through the IPS trampoline) each time the game has stored a view matrix ---- */

void wf_cam_hook(u64 view)
{
    static u64 active, last;
    static float eye[3], yaw, pitch;
    if (!g_cheat[5]) { active = 0; return; }
    u64 b = g_base;
    if (!view || *(u64 *)view != b + VIEW_VT || gameplay_view() != view) return;
    u64 now = now_ms(), el = now - last;
    if (active != view || el > 500) {
        float t[3];
        for (int i = 0; i < 3; i++) {
            eye[i] = (float)*(volatile double *)(view + 96 + i * 8);
            t[i] = (float)*(volatile double *)(view + 120 + i * 8);
        }
        float x = eye[0] - t[0], y = eye[1] - t[1], z = eye[2] - t[2];
        yaw = fatan2_(x, z);
        pitch = fatan2_(y, fsqrt_(x * x + z * z));
        active = view;
    }
    last = now;
    float dt = (el > 0 && el < 100) ? (float)el / 1000.f : 0.f;
    yaw -= 1.6f * dt * stick(g_stk[2]);
    pitch -= 1.2f * dt * stick(g_stk[3]);
    if (pitch > 1.55f) pitch = 1.55f;
    if (pitch < -1.55f) pitch = -1.55f;
    float sy = fsin_(yaw), cy = fcos_(yaw), sp = fsin_(pitch), cp = fcos_(pitch);
    float ax[3][3] = {{cy, 0.f, -sy}, {-sp * sy, cp, -sp * cy}, {cp * sy, sp, cp * cy}};
    float step = dt * ((g_held & 1) ? 30.f : 8.f);   /* hold A for a faster camera */
    float right = stick(g_stk[0]), forward = stick(g_stk[1]);
    float height = (float)((int)((g_held >> 9) & 1) - (int)((g_held >> 8) & 1));
    eye[1] += step * height;
    for (int i = 0; i < 3; i++) eye[i] += step * (right * ax[0][i] - forward * ax[2][i]);
    for (int row = 0; row < 3; row++) {
        float v[4];
        v[0] = ax[row][0]; v[1] = ax[row][1]; v[2] = ax[row][2];
        v[3] = -(ax[row][0] * eye[0] + ax[row][1] * eye[1] + ax[row][2] * eye[2]);
        for (int col = 0; col < 4; col++) *(volatile float *)(view + 8 + (row * 4 + col) * 4) = v[col];
    }
}
