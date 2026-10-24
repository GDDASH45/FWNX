
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    . = 0x00100000;

    kernel_start = .;

    .text ALIGN(16) :
    {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }

    .rodata ALIGN(16) :
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(16) :
    {
        *(.data .data.*)
    }

    
    .bss ALIGN(16) :
    {
        bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(16);
        . += 16384;
        stack_top = .;
        bss_end = .;
    }


    . = ALIGN(4096);
    kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
