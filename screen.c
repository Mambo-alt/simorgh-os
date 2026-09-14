#include "screen.h"

static int cursor_row = 0;
static int cursor_col = 0;
static int min_row = 0;
static int min_col = 0;

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(void) {
    unsigned short pos = cursor_row * 80 + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

void set_input_boundary(void) {
    min_row = cursor_row;
    min_col = cursor_col;
}

void putchar(char c) {
    char *video_memory = (char*) 0xb8000;

    if (cursor_row >= 25) {
        for (int i = 0; i < 24 * 80 * 2; i++) {
            video_memory[i] = video_memory[i + 80 * 2];
        }
        for (int i = 24 * 80 * 2; i < 25 * 80 * 2; i += 2) {
            video_memory[i] = ' ';
            video_memory[i + 1] = 0x0f;
        }
        cursor_row = 24;
        if (min_row > 0) {
            min_row--;
        }
    }

    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
        update_cursor();
        return;
    }

    if (c == '\b') {
        if (cursor_row > min_row || (cursor_row == min_row && cursor_col > min_col)) {
            if (cursor_col > 0) {
                cursor_col--;
            } else {
                cursor_row--;
                cursor_col = 79;
            }
            int offset = (cursor_row * 80 + cursor_col) * 2;
            video_memory[offset] = ' ';
            video_memory[offset + 1] = 0x0f;
            update_cursor();
        }
        return;
    }

    int offset = (cursor_row * 80 + cursor_col) * 2;
    video_memory[offset] = c;
    video_memory[offset + 1] = 0x0f;

    cursor_col++;
    if (cursor_col >= 80) {
        cursor_col = 0;
        cursor_row++;
    }
    update_cursor();
}

void print(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        putchar(str[i]);
        i++;
    }
}

void clear_screen(void) {
    char *video_memory = (char*) 0xb8000;
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i + 1] = 0x0f;
    }
    cursor_row = 0;
    cursor_col = 0;
    min_row = 0;
    min_col = 0;
    update_cursor();
}
