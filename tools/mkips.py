"""Generate the IPS32 patch for Wayfarer (Ocarina of Time 1.1.1, main build E0F3...3348).

Each hook replaces one `bl` in main with a `bl` to a 20-byte trampoline in the padding
after main's text. The trampoline jumps through a bridge-table slot when it is non-zero,
otherwise falls through to the original call target.

Offsets below are IPS offsets: Atmosphere adds the 0x100-byte NSO header, so
memory offset = IPS offset - 0x100. All encoding is done in memory offsets.
"""
import struct
import sys

BUILD_ID = "E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348"
NSO_HDR = 0x100
# Where trampolines may go: the parts of main's padding (memory offsets, 0x3C461D0-0x3C47000) that
# neither Lichii's nor Fritchii's (v6.5) built-in Ocarina patches write. A built-in skips itself when
# a user mod has touched one of its bytes, so staying inside these gaps matters. The last gap is also
# used by a community ultrawide mod and by some cheats (0x3C46CC0-0x3C46DB0): see docs/RESEARCH.md.
GAPS = (
    (0x3C461D0, 0x3C46200),
    (0x3C46290, 0x3C462C0),
    (0x3C462CC, 0x3C46300),
    (0x3C46C00, 0x3C46DC0),
)
BRIDGE = 0x4E9F000                  # page of the bridge table (memory offset)

# (site IPS offset, slot offset in bridge page, original bl target IPS offset)
NPAD_HOOKS = [
    (0x34695F8, 0x800, 0x3C428F0),
    (0x3469624, 0x808, 0x3C42910),
    (0x3469650, 0x810, 0x3C42920),
    (0x346976C, 0x818, 0x3C42900),
    (0x3469798, 0x820, 0x3C42930),
    # NVN loader (resolves the graphics API); the module wraps present from here.
    (0x346C0F0, 0x828, 0x129E390),
    (0x346C1D4, 0x828, 0x129E390),
]


def b(pc, target, link=False):
    rel = (target - pc) >> 2
    assert -(1 << 25) <= rel < (1 << 25)
    return struct.pack("<I", (0x94000000 if link else 0x14000000) | (rel & 0x3FFFFFF))


def adrp(pc, rd, target_page):
    imm = (target_page - (pc & ~0xFFF)) >> 12
    return struct.pack("<I", 0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7FFFF) << 5) | rd)


def trampoline(pc, slot, fallback):
    """adrp x16,page; ldr x16,[x16,#slot]; cbz x16,+8; br x16; b fallback"""
    code = adrp(pc, 16, BRIDGE)
    code += struct.pack("<I", 0xF9400000 | ((slot // 8) << 10) | (16 << 5) | 16)
    code += struct.pack("<I", 0xB4000000 | (2 << 5) | 16)
    code += struct.pack("<I", 0xD61F0200)
    code += b(pc + 16, fallback)
    return code


ACTOR_SITE = 0x49D9C0     # IPS offset: the player actor update function's first instruction (pacibsp)
ACTOR_BODY = 0x49D8C4     # memory offset of its second instruction
ACTOR_SLOT = 0x848
LIFE_SITE = 0x49DDF8      # IPS offset: `cas w11, w12, [x13]` that writes Link's life
LIFE_BACK = 0x49DCFC      # memory offset of the instruction after it
LIFE_SLOT = 0x870         # data slot: address of Link's life word while God Mode is on, else 0


def actor_trampoline(pc):
    code = adrp(pc, 16, BRIDGE)
    code += struct.pack("<I", 0xF9400000 | ((ACTOR_SLOT // 8) << 10) | (16 << 5) | 16)
    code += struct.pack("<I", 0xB4000000 | (2 << 5) | 16)
    code += struct.pack("<I", 0xD61F0200)
    code += struct.pack("<I", 0xD503237F)          # pacibsp (the instruction replaced at the site)
    code += b(pc + 20, ACTOR_BODY)
    return code


def life_trampoline(pc):
    """If the slot holds this CAS's address, never let the life word go down."""
    code = struct.pack("<I", 0xA9BF47F0)           # stp x16, x17, [sp, #-16]!
    code += adrp(pc + 4, 16, BRIDGE)
    code += struct.pack("<I", 0xF9400000 | ((LIFE_SLOT // 8) << 10) | (16 << 5) | 16)
    code += struct.pack("<I", 0xEB0D021F)          # cmp x16, x13
    code += struct.pack("<I", 0x54000081)          # b.ne +16
    code += struct.pack("<I", 0x7100017F)          # cmp w11, #0
    code += struct.pack("<I", 0x7A4CC164)          # ccmp w11, w12, #4, gt
    code += struct.pack("<I", 0x1A8CC16C)          # csel w12, w11, w12, gt
    code += struct.pack("<I", 0xA8C147F0)          # ldp x16, x17, [sp], #16
    code += struct.pack("<I", 0x88AB7DAC)          # cas w11, w12, [x13]
    code += b(pc + 40, LIFE_BACK)
    return code


CAM_SITE = 0x4C5068 + NSO_HDR   # IPS offset: `ldr x8, [x19, #0x20]` right after the view matrix is stored (x20 = view)
CAM_ORIG = 0xF9401268           # that instruction
CAM_BACK = 0x4C506C             # memory offset of the instruction after it
CAM_SLOT = 0x878                # hook(view)


def camera_trampoline(pc):
    """Saves every register, calls the hook with x0 = x20 (the view), restores, runs the replaced instruction."""
    w = []
    w.append(0xD10C03FF)                                              # sub sp, sp, #0x300
    for i in range(0, 30, 2):                                         # x0..x29 at sp+0..0xE8
        w.append(0xA9000000 | ((i * 8 // 8) << 15) | ((i + 1) << 10) | (31 << 5) | i)
    w.append(0xF9000000 | ((0xF0 // 8) << 10) | (31 << 5) | 30)       # str x30, [sp, #0xF0]
    for i in range(0, 32, 2):                                         # q0..q31 at sp+0x100..
        w.append(0xAD000000 | ((16 + i) << 15) | ((i + 1) << 10) | (31 << 5) | i)
    w.append(0xAA1403E0)                                              # mov x0, x20
    code = b"".join(struct.pack("<I", x) for x in w)
    here = pc + len(code)
    code += adrp(here, 16, BRIDGE)
    code += struct.pack("<I", 0xF9400000 | ((CAM_SLOT // 8) << 10) | (16 << 5) | 16)   # ldr x16, [x16, #slot]
    code += struct.pack("<I", 0xB4000000 | (2 << 5) | 16)                              # cbz x16, +8
    code += struct.pack("<I", 0xD63F0200)                                              # blr x16
    w = []
    for i in range(0, 30, 2):
        w.append(0xA9400000 | ((i * 8 // 8) << 15) | ((i + 1) << 10) | (31 << 5) | i)
    w.append(0xF9400000 | ((0xF0 // 8) << 10) | (31 << 5) | 30)       # ldr x30, [sp, #0xF0]
    for i in range(0, 32, 2):
        w.append(0xAD400000 | ((16 + i) << 15) | ((i + 1) << 10) | (31 << 5) | i)
    w.append(0x910C03FF)                                              # add sp, sp, #0x300
    w.append(CAM_ORIG)
    code += b"".join(struct.pack("<I", x) for x in w)
    code += b(pc + len(code), CAM_BACK)
    return code


def layout():
    """Give each trampoline an address: first gap with room, biggest pieces first."""
    free = [list(gap) for gap in GAPS]

    def take(size):
        for gap in free:
            if gap[0] + size <= gap[1]:
                gap[0] += size
                return gap[0] - size
        raise ValueError("no room left for a %d-byte trampoline" % size)

    camera = take(len(camera_trampoline(GAPS[-1][0])))   # its length does not depend on its address
    life = take(44)
    actor = take(24)
    npad = [take(20) for _ in NPAD_HOOKS]
    return camera, life, actor, npad


CAMERA_AT, LIFE_AT, ACTOR_AT, NPAD_AT = layout()
# memory offset (from main) of the actor trampoline's fallback (pacibsp; b body)
ACTOR_FALLBACK = ACTOR_AT + 16


def records():
    recs = []
    for pc, (site, slot, orig) in zip(NPAD_AT, NPAD_HOOKS):
        site_m, orig_m = site - NSO_HDR, orig - NSO_HDR
        recs.append((site, b(site_m, pc, link=True)))
        recs.append((pc + NSO_HDR, trampoline(pc, slot, orig_m)))
    recs.append((ACTOR_SITE, b(ACTOR_SITE - NSO_HDR, ACTOR_AT)))
    recs.append((ACTOR_AT + NSO_HDR, actor_trampoline(ACTOR_AT)))
    recs.append((LIFE_SITE, b(LIFE_SITE - NSO_HDR, LIFE_AT)))
    recs.append((LIFE_AT + NSO_HDR, life_trampoline(LIFE_AT)))
    recs.append((CAM_SITE, b(CAM_SITE - NSO_HDR, CAMERA_AT)))
    recs.append((CAMERA_AT + NSO_HDR, camera_trampoline(CAMERA_AT)))
    return recs


def build():
    out = b"IPS32"
    for off, data in records():
        out += struct.pack(">I", off) + struct.pack(">H", len(data)) + data
    return out + b"EEOF"


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else BUILD_ID + ".ips"
    open(path, "wb").write(build())
    print("wrote", path)
