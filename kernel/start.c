
#include <stddef.h>

void kernel_main(void)
{
    /*
     * FWNX kernel initialization begins here.
     *
     * Interrupts remain disabled until the interrupt
     * descriptor table and interrupt handlers are ready.
     */

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
