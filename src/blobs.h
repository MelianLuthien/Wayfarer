/* Art embedded in read-only data: the menu's fonts (build/art/fonts.bin, tools/genfont.py) and its
   frame and parchment (build/art/panel.bin, tools/packframe.py), found through the -I path build.py
   passes. Code reaches them PC-relative, so nothing here needs relocation. */
__asm__(
    ".section .rodata.fonts,\"a\"\n"
    ".balign 16\n"
    ".global wf_font_blob\n"
    "wf_font_blob:\n"
    ".incbin \"fonts.bin\"\n"
    ".balign 16\n"
    ".global wf_panel_blob\n"
    "wf_panel_blob:\n"
    ".incbin \"panel.bin\"\n"
    ".balign 16\n"
    ".global wf_logo_blob\n"
    "wf_logo_blob:\n"
    ".incbin \"logo.bin\"\n"
    ".previous\n");
extern const u8 wf_font_blob[];
extern const u8 wf_panel_blob[];
extern const u8 wf_logo_blob[];
