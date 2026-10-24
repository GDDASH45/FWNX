
.section .text.entry
.code32

.global _start
.type _start, @function

.extern kernel_main
.extern bss_start
.extern bss_end
.extern stack_top

_start:
    cli
    cld

    movl $stack_top, %esp
    xorl %ebp, %ebp

    /* Clear the complete BSS section. */
    movl $bss_start, %edi
    movl $bss_end, %ecx
    subl %edi, %ecx
    xorl %eax, %eax
    rep stosb

    call kernel_main

1:
    cli
    hlt
    jmp 1b

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
