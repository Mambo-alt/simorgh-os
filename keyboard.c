#include "keyboard.h"
#include "screen.h"
#include "shell.h"

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int shift_pressed = 0;
static char cmd_buffer[128];
static int cmd_len = 0;

static const char scancode_ascii[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

static const char scancode_ascii_shift[] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};

void keyboard_handler(void) {
    unsigned char scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        outb(0x20, 0x20);
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        outb(0x20, 0x20);
        return;
    }

    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_ascii)) {
            char c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];

            if (c == '\n') {
                cmd_buffer[cmd_len] = '\0';
                shell_execute(cmd_buffer);
                cmd_len = 0;
            }
            else if (c == '\b') {
                if (cmd_len > 0) {
                    cmd_len--;
                    putchar('\b');
                }
            }
            else if (c != 0) {
                if (cmd_len < 127) {
                    cmd_buffer[cmd_len] = c;
                    cmd_len++;
                    putchar(c);
                }
            }
        }
    }

    outb(0x20, 0x20);
}
