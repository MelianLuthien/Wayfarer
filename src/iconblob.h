/* The icon art, embedded in read-only data. The file is build/art/icons64.bin (see tools/packicons.py),
   found through the -I path build.py passes. Code reaches it through PC-relative addressing, so
   nothing here needs relocation. */
__asm__(
    ".section .rodata.icons,\"a\"\n"
    ".balign 16\n"
    ".global wf_icon_blob\n"
    "wf_icon_blob:\n"
    ".incbin \"icons64.bin\"\n"
    ".global wf_icon_blob_end\n"
    "wf_icon_blob_end:\n"
    ".previous\n");
extern const u8 wf_icon_blob[];
extern const u8 wf_icon_blob_end[];

static u64 icons_checksum(void)
{
    u64 s = 0;
    for (const u8 *p = wf_icon_blob; p + 8 <= wf_icon_blob_end; p += 8) s = s * 31 + *(const u64 *)p;
    return s;
}
