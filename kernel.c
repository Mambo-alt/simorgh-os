#include "screen.h"
#include "idt.h"
#include "shell.h"

void kernel_main(void) {
    clear_screen();
    print("Welcome to the Simorgh OS in C!\n");

    idt_init();
    shell_init();

    while (1) {
    }
}
