
#include "screen.h"
#include "idt.h"
#include "keyboard.h"
#include "shell.h"

void kernel_main(void) {
    clear_screen();
    print("Welcome to the Simorgh OS!\n");

    keyboard_init();
    idt_init();

    shell_init();

    print("Ready\n");

    while (1) {
        key_event_t ev;
        int any = 0;

        while (keyboard_poll(&ev)) {
            shell_handle_event(&ev);
            any = 1;
        }

        if (!any) {
            asm volatile ("hlt");
        }
    }
}
