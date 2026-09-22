#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_ENTER,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_DELETE,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_ESC,
    KEY_CTRL_C,
    KEY_CTRL_L,
    KEY_CTRL_U,
    KEY_CTRL_W,
    KEY_CTRL_A,
    KEY_CTRL_E,
} key_type_t;

typedef struct {
    key_type_t    type;
    char          ch;
    unsigned char mods;
} key_event_t;

void keyboard_init(void);
void keyboard_handler(void);
int  keyboard_poll(key_event_t *out);

#endif
