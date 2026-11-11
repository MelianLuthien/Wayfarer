/*
 * Wayfarer: stage 1. Loads as a Switch ExeFS module (subsdk8), finds the game's
 * `main`, arms five controller hooks and logs a line when R3 is pressed.
 *
 * The hook plumbing (IPS trampolines + bridge table in main's bss) follows the
 * layout reverse-engineered from the Hyrule Cheat Menu rc1; see docs/RESEARCH.md.
 * GPL-3.0.
 */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;

#define TITLE_ID        0x0400486024DEC000ull
#define MAIN_RX_SIZE    0x3c47000ull   /* size of main's executable region */
#define MAIN_MOD0_WORD  0x3c4705cu     /* word at main+4: offset of MOD0 */
#define BRIDGE_OFFSET   0x4E9F800ull   /* bridge table inside main's bss */
#define BRIDGE_BYTES    0x78ull

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

/* ---- state ------------------------------------------------------------ */

static u64 g_base;          /* runtime address of main */
static u32 g_prev[5];       /* previous R3 state per hooked input function */

/* Offsets (from main) of the five input functions the hooks call through to. */
const u64 wf_npad_fn[5] = {0x3c427f0, 0x3c42810, 0x3c42820, 0x3c42800, 0x3c42830};

/* ---- the hook --------------------------------------------------------- */

/*
 * Replaces a call to one of the game's pad-state readers
 *   long fn(State *out, int count, const u32 *npad_id)
 * Calls the original, then looks at the newest sample for R3 (StickR, bit 5).
 */
long wf_npad(void *out, int count, const u32 *id, u64 fn_off, u64 idx)
{
    long (*orig)(void *, int, const u32 *) = (long (*)(void *, int, const u32 *))(g_base + fn_off);
    long n = orig(out, count, id);
    if (n >= 1 && id && (*id == 0 || *id == 0x20)) {
        u64 buttons = *(const u64 *)((const u8 *)out + 8);
        u32 r3 = (u32)((buttons >> 5) & 1);
        if (r3 && !g_prev[idx]) dbg("[wayfarer] R3 pressed\n");
        g_prev[idx] = r3;
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

/* ---- entry ------------------------------------------------------------ */

void wf_init(void)
{
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
    __asm__ volatile("dmb ish" ::: "memory");
    dbg("[wayfarer] stage 1 loaded; controller hooks armed (press R3)\n");
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
    "  .quad 12, wf_init\n"            /* DT_INIT */
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
