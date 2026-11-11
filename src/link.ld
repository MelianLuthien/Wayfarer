ENTRY(_start)
SECTIONS
{
    . = 0;
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }
    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata .rodata.*)
    }
    . = ALIGN(0x1000);
    .data : {
        *(.data.mod0)
        *(.data .data.*)
    }
    . = ALIGN(0x1000);
    __bss_start = .;
    .bss : {
        KEEP(*(.bss.modobj))
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(8);
    __bss_end = .;
    /DISCARD/ : {
        *(.comment) *(.eh_frame*) *(.note*) *(.ARM.attributes) *(.symtab) *(.strtab)
    }
}
