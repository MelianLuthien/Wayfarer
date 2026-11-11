"""Build Wayfarer: compile src/wayfarer.c with Zig, wrap the result as an NSO (subsdk8),
and write the IPS patch. Output goes to dist/exefs/.

usage: python tools/build.py [--zig PATH]
"""
import argparse
import hashlib
import os
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import mkips  # noqa: E402


def elf_sections(data):
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    secs = []
    for i in range(shnum):
        name, typ, flags, addr, off, size = struct.unpack_from("<IIQQQQ", data, shoff + i * shentsize)
        secs.append([name, typ, addr, off, size])
    stroff = secs[shstrndx][3]
    out = {}
    for name, typ, addr, off, size in secs:
        end = data.index(b"\0", stroff + name)
        out[data[stroff + name:end].decode()] = (typ, addr, off, size)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zig", default=os.environ.get("ZIG", "zig"))
    ap.add_argument("--variant", type=int, default=2, help="0 empty init, 1 log only, 2 full")
    args = ap.parse_args()

    build = os.path.join(ROOT, "build")
    dist = os.path.join(ROOT, "dist" if args.variant == 2 else "dist-variant%d" % args.variant, "exefs")
    os.makedirs(build, exist_ok=True)
    os.makedirs(dist, exist_ok=True)
    elf = os.path.join(build, "wayfarer.elf")

    # Pack the icon art into build/art/icons64.bin and src/icons.h (empty when there is no art yet).
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "packicons.py")], check=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "genfont.py")], check=True)
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "packframe.py")], check=True)

    cmd = [
        args.zig, "cc", "-target", "aarch64-freestanding", "-mcpu=cortex_a57", "-O2",
        "-I", os.path.join(build, "art"),
        "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pic", "-fno-pie",
        "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Wall", "-Wextra",
        "-DWF_VARIANT=%d" % args.variant, "-nostdlib", "-static", "-Wl,-T," + os.path.join(ROOT, "src", "link.ld"),
        "-Wl,--build-id=none", "-o", elf, os.path.join(ROOT, "src", "wayfarer.c"),
    ]
    subprocess.run(cmd, check=True)

    data = open(elf, "rb").read()
    secs = elf_sections(data)

    def sec(name):
        if name not in secs:
            return 0, b""
        typ, addr, off, size = secs[name]
        return addr, (data[off:off + size] if typ != 8 else b"\0" * size)

    text_a, text = sec(".text")
    ro_a, ro = sec(".rodata")
    data_a, dat = sec(".data")
    bss_a, bss = sec(".bss")
    bss_end = max(bss_a + len(bss), data_a + len(dat))
    bss_start = (data_a + len(dat) + 0xFFF) & ~0xFFF
    # The NSO header counts bss from the end of the data segment (unaligned), not from the
    # page-aligned start the linker uses; otherwise the loader maps too few pages.
    bss_size = bss_end - (data_a + len(dat))
    assert text_a == 0 and ro_a % 0x1000 == 0 and data_a % 0x1000 == 0, (text_a, ro_a, data_a)

    build_id = hashlib.sha256(text + ro + dat).digest()[:0x20]
    hdr = bytearray(0x100)
    hdr[0:4] = b"NSO0"
    struct.pack_into("<I", hdr, 0x0C, 0x38)  # segments stored uncompressed, hashes checked
    file_off = 0x100
    for i, (mem, seg) in enumerate(((text_a, text), (ro_a, ro), (data_a, dat))):
        struct.pack_into("<III", hdr, 0x10 + 0x10 * i, file_off, mem, len(seg))
        struct.pack_into("<I", hdr, 0x60 + 4 * i, len(seg))
        hdr[0xA0 + 0x20 * i:0xC0 + 0x20 * i] = hashlib.sha256(seg).digest()
        file_off += len(seg)
    struct.pack_into("<I", hdr, 0x3C, bss_size)
    hdr[0x40:0x60] = build_id

    out = os.path.join(dist, "subsdk8")
    open(out, "wb").write(bytes(hdr) + text + ro + dat)
    ips = os.path.join(dist, mkips.BUILD_ID + ".ips")
    open(ips, "wb").write(mkips.build())
    print("text %#x ro %#x@%#x data %#x@%#x bss %#x" % (len(text), len(ro), ro_a, len(dat), data_a, bss_size))
    print("wrote", out)
    print("wrote", ips)


if __name__ == "__main__":
    main()
