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
CAVE = 0x3C46380 - NSO_HDR          # first trampoline (memory offset)
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


def records():
    recs = []
    pc = CAVE
    for site, slot, orig in NPAD_HOOKS:
        site_m, orig_m = site - NSO_HDR, orig - NSO_HDR
        recs.append((site, b(site_m, pc, link=True)))
        recs.append((pc + NSO_HDR, trampoline(pc, slot, orig_m)))
        pc += 20
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
