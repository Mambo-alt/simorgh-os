#ifndef SCREEN_H
#define SCREEN_H

void clear_screen(void);
void screen_init(void);
void print(const char *str);
void putchar(char c);
void set_input_boundary(void);
void screen_get_cursor(int *row, int *col);
void screen_set_cursor(int row, int col);

void screen_scroll(int lines);
void screen_scroll_top(void);
void screen_scroll_bottom(void);
int  screen_is_scrolled(void);

#endif
