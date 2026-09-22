#include "screen.h"

#define SCREEN_ROWS 25
#define SCREEN_COLS 80
#define SB_MAX      200

static int cursor_row = 0;
static int cursor_col = 0;
static int min_row = 0;
static int min_col = 0;

static char sb_buf[SB_MAX][SCREEN_COLS];
static int  sb_count = 0;
static int  sb_start = 0;

static char saved_screen[SCREEN_ROWS][SCREEN_COLS];
static int  saved_cursor_row = 0;
static int  saved_cursor_col = 0;
static int  saved_min_row = 0;
static int  saved_min_col = 0;
static int  saved_valid = 0;

static int  scroll_offset = 0;

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void update_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0E);
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0F);

    unsigned short pos = cursor_row * SCREEN_COLS + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

static void hide_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

static void push_scrollback_row(int screen_row) {
    char *vga = (char *)0xB8000;
    int idx;
    if (sb_count < SB_MAX) {
        idx = (sb_start + sb_count) % SB_MAX;
        sb_count++;
    } else {
        idx = sb_start;
        sb_start = (sb_start + 1) % SB_MAX;
    }
    for (int i = 0; i < SCREEN_COLS; i++) {
        sb_buf[idx][i] = vga[(screen_row * SCREEN_COLS + i) * 2];
    }
}

static void save_current_screen(void) {
    char *vga = (char *)0xB8000;
    for (int r = 0; r < SCREEN_ROWS; r++) {
        for (int c = 0; c < SCREEN_COLS; c++) {
            saved_screen[r][c] = vga[(r * SCREEN_COLS + c) * 2];
        }
    }
    saved_cursor_row = cursor_row;
    saved_cursor_col = cursor_col;
    saved_min_row = min_row;
    saved_min_col = min_col;
    saved_valid = 1;
}

static void redraw_scroll_view(void) {
    char *vga = (char *)0xB8000;

    if (scroll_offset == 0) {
        if (saved_valid) {
            for (int r = 0; r < SCREEN_ROWS; r++) {
                for (int c = 0; c < SCREEN_COLS; c++) {
                    vga[(r * SCREEN_COLS + c) * 2]     = saved_screen[r][c];
                    vga[(r * SCREEN_COLS + c) * 2 + 1] = 0x0F;
                }
            }
            cursor_row = saved_cursor_row;
            cursor_col = saved_cursor_col;
            min_row    = saved_min_row;
            min_col    = saved_min_col;
            saved_valid = 0;
            update_cursor();
        }
        return;
    }

    int total = sb_count + SCREEN_ROWS;
    int end_line = total - scroll_offset;
    int start_line = end_line - SCREEN_ROWS;
    if (start_line < 0) start_line = 0;

    for (int r = 0; r < SCREEN_ROWS; r++) {
        int logical = start_line + r;
        const char *src = 0;

        if (logical < sb_count) {
            int idx = (sb_start + logical) % SB_MAX;
            src = sb_buf[idx];
        } else if (logical < total) {
            src = saved_screen[logical - sb_count];
        }

        for (int c = 0; c < SCREEN_COLS; c++) {
            if (src) {
                vga[(r * SCREEN_COLS + c) * 2] = src[c];
            } else {
                vga[(r * SCREEN_COLS + c) * 2] = ' ';
            }
            vga[(r * SCREEN_COLS + c) * 2 + 1] = 0x07;
        }
    }

    hide_cursor();
}

int screen_is_scrolled(void) {
    return scroll_offset > 0;
}

void screen_scroll(int lines) {
    if (lines == 0) return;
    if (lines > 0 && sb_count == 0) return;

    if (scroll_offset == 0 && lines > 0) {
        save_current_screen();
    }

    scroll_offset += lines;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset > sb_count) scroll_offset = sb_count;

    redraw_scroll_view();
}

void screen_scroll_top(void) {
    if (sb_count == 0) return;
    if (scroll_offset == 0) save_current_screen();
    scroll_offset = sb_count;
    redraw_scroll_view();
}

void screen_scroll_bottom(void) {
    if (scroll_offset == 0) return;
    scroll_offset = 0;
    redraw_scroll_view();
}

void set_input_boundary(void) {
    min_row = cursor_row;
    min_col = cursor_col;
}

void screen_get_cursor(int *row, int *col) {
    *row = cursor_row;
    *col = cursor_col;
}

void screen_set_cursor(int row, int col) {
    if (row < 0)  row = 0;
    if (row > 24) row = 24;
    if (col < 0)  col = 0;
    if (col > 79) col = 79;
    cursor_row = row;
    cursor_col = col;
    update_cursor();
}

void putchar(char c) {
    char *video_memory = (char*) 0xb8000;

    if (cursor_row >= SCREEN_ROWS) {
        push_scrollback_row(0);

        for (int i = 0; i < (SCREEN_ROWS - 1) * SCREEN_COLS * 2; i++) {
            video_memory[i] = video_memory[i + SCREEN_COLS * 2];
        }
        for (int i = (SCREEN_ROWS - 1) * SCREEN_COLS * 2;
             i < SCREEN_ROWS * SCREEN_COLS * 2; i += 2) {
            video_memory[i]     = ' ';
            video_memory[i + 1] = 0x0F;
        }
        cursor_row = SCREEN_ROWS - 1;
        if (min_row > 0) min_row--;
    }

    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
        update_cursor();
        return;
    }

    if (c == '\b') {
        if (cursor_row > min_row ||
            (cursor_row == min_row && cursor_col > min_col)) {
            if (cursor_col > 0) {
                cursor_col--;
            } else {
                cursor_row--;
                cursor_col = SCREEN_COLS - 1;
            }
            int offset = (cursor_row * SCREEN_COLS + cursor_col) * 2;
            video_memory[offset]     = ' ';
            video_memory[offset + 1] = 0x0F;
            update_cursor();
        }
        return;
    }

    int offset = (cursor_row * SCREEN_COLS + cursor_col) * 2;
    video_memory[offset]     = c;
    video_memory[offset + 1] = 0x0F;
    cursor_col++;
    if (cursor_col >= SCREEN_COLS) {
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
    for (int i = 0; i < SCREEN_ROWS * SCREEN_COLS * 2; i += 2) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = 0x0F;
    }
    cursor_row = 0;
    cursor_col = 0;
    min_row    = 0;
    min_col    = 0;
    update_cursor();
}

void screen_init(void) {
    sb_count      = 0;
    sb_start      = 0;
    scroll_offset = 0;
    saved_valid   = 0;
    clear_screen();
}
