import struct, sys, lz4.block

def load_nso(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'NSO0'
    flags = struct.unpack_from('<I', d, 0xC)[0]
    segs = []
    csz = struct.unpack_from('<3I', d, 0x60)
    for i in range(3):
        fo, mo, ds = struct.unpack_from('<3I', d, 0x10 + 0x10 * i)
        raw = d[fo:fo + csz[i]]
        if flags & (1 << i):
            raw = lz4.block.decompress(raw, uncompressed_size=ds)
        segs.append((mo, raw))
    return segs, d[0x40:0x60].hex().upper().rstrip('0')

if __name__ == '__main__':
    segs, bid = load_nso(sys.argv[1])
    print(bid)
    out = open(sys.argv[2], 'wb')
    # flat image: text, ro, data at their mem offsets
    end = max(mo + len(r) for mo, r in segs)
    img = bytearray(end)
    for mo, r in segs:
        img[mo:mo + len(r)] = r
        print(hex(mo), hex(len(r)))
    out.write(img)
