import struct, re, sys
import numpy as np

class PE:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        d = self.d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        assert d[pe:pe+4] == b'PE\0\0'
        nsec = struct.unpack_from('<H', d, pe+6)[0]
        optsz = struct.unpack_from('<H', d, pe+20)[0]
        opt = pe + 24
        self.base = struct.unpack_from('<Q', d, opt+24)[0]
        so = opt + optsz
        self.secs = []
        for i in range(nsec):
            n, vs, va, rs, ro = struct.unpack_from('<8sIIII', d, so + 40*i)
            self.secs.append((n.rstrip(b'\0').decode(), va, vs, ro, rs))

    def off2rva(self, off):
        for n, va, vs, ro, rs in self.secs:
            if ro <= off < ro + rs:
                return va + (off - ro)

    def rva2off(self, rva):
        for n, va, vs, ro, rs in self.secs:
            if va <= rva < va + max(vs, rs):
                return ro + (rva - va)

    def text(self):
        for s in self.secs:
            if s[0] == '.text':
                return s

    def xrefs_rip(self, target_rva):
        """find rip-relative references (lea/mov/call ...) to target rva in .text"""
        n, va, vs, ro, rs = self.text()
        raw = self.d[ro:ro+rs]
        n4 = rs - 4
        disp = np.ndarray(shape=(n4,), dtype='<i4', buffer=raw, strides=(1,)).astype(np.int64)
        res = []
        pos = np.arange(n4, dtype=np.int64)
        for extra in (0, 1, 4):
            hit = np.where(va + pos + 4 + extra + disp == target_rva)[0]
            for p in hit:
                p = int(p)
                if p >= 1 and (raw[p-1] & 0xC7) == 0x05:
                    res.append(va + p - 3)  # approx insn start (rva)
        return sorted(set(res))

def strings_find(pe, s):
    out = []
    i = -1
    while True:
        i = pe.d.find(s, i + 1)
        if i < 0:
            break
        out.append(i)
    return out
