#include "keyboard.h"

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static unsigned char shift_pressed = 0;
static unsigned char ctrl_pressed  = 0;
static unsigned char alt_pressed   = 0;
static unsigned char caps_lock     = 0;
static unsigned char extended      = 0;

#define QSIZE 64
static key_event_t q[QSIZE];
static unsigned int q_head = 0;
static unsigned int q_tail = 0;

static void q_push(key_event_t ev) {
    unsigned int next = (q_head + 1) % QSIZE;
    if (next == q_tail) return;
    q[q_head] = ev;
    q_head = next;
}

int keyboard_poll(key_event_t *out) {
    if (q_tail == q_head) return 0;
    *out = q[q_tail];
    q_tail = (q_tail + 1) % QSIZE;
    return 1;
}

static const char map_lower[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',0,
    '*', 0,  ' '
};

static const char map_upper[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?',0,
    '*', 0,  ' '
};

static key_type_t ext_lookup(unsigned char sc) {
    switch (sc) {
        case 0x48: return KEY_UP;
        case 0x50: return KEY_DOWN;
        case 0x4B: return KEY_LEFT;
        case 0x4D: return KEY_RIGHT;
        case 0x47: return KEY_HOME;
        case 0x4F: return KEY_END;
        case 0x53: return KEY_DELETE;
        case 0x49: return KEY_PAGE_UP;
        case 0x51: return KEY_PAGE_DOWN;
        default:   return KEY_NONE;
    }
}

void keyboard_handler(void) {
    unsigned char sc = inb(0x60);

    if (sc == 0xE0) { extended = 1; outb(0x20, 0x20); return; }

    if (extended) {
        extended = 0;
        if (!(sc & 0x80)) {
            key_type_t t = ext_lookup(sc);
            if (t != KEY_NONE) {
                key_event_t ev;
                ev.type = t;
                ev.ch   = 0;
                ev.mods = 0;
                if (shift_pressed) ev.mods |= 1;
                if (ctrl_pressed)  ev.mods |= 2;
                q_push(ev);
            }
        }
        outb(0x20, 0x20);
        return;
    }

    if (sc & 0x80) {
        unsigned char mk = sc & 0x7F;
        if (mk == 0x2A || mk == 0x36) shift_pressed = 0;
        else if (mk == 0x1D)          ctrl_pressed  = 0;
        else if (mk == 0x38)          alt_pressed   = 0;
        outb(0x20, 0x20);
        return;
    }

    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; outb(0x20, 0x20); return; }
    if (sc == 0x1D)               { ctrl_pressed  = 1; outb(0x20, 0x20); return; }
    if (sc == 0x38)               { alt_pressed   = 1; outb(0x20, 0x20); return; }
    if (sc == 0x3A)               { caps_lock ^= 1;    outb(0x20, 0x20); return; }

    if (sc >= 0x3B && sc <= 0x44) { outb(0x20, 0x20); return; }

    if (sc < 128) {
        char c = shift_pressed ? map_upper[sc] : map_lower[sc];

        if (c >= 'a' && c <= 'z' && (caps_lock ^ shift_pressed))
            c = map_upper[sc];

        if (ctrl_pressed && c >= 'a' && c <= 'z') c = c - 'a' + 1;
        if (ctrl_pressed && c >= 'A' && c <= 'Z') c = c - 'A' + 1;

        if (c != 0) {
            key_event_t ev;
            ev.ch   = c;
            ev.mods = 0;
            if (shift_pressed) ev.mods |= 1;
            if (ctrl_pressed)  ev.mods |= 2;

            if      (c == '\n')               ev.type = KEY_ENTER;
            else if (c == '\b')               ev.type = KEY_BACKSPACE;
            else if (c == '\t')               ev.type = KEY_TAB;
            else if (c == 27)                 ev.type = KEY_ESC;
            else if (c == 3)                  ev.type = KEY_CTRL_C;
            else if (c == 12)                 ev.type = KEY_CTRL_L;
            else if (c == 21)                 ev.type = KEY_CTRL_U;
            else if (c == 23)                 ev.type = KEY_CTRL_W;
            else if (c == 1)                  ev.type = KEY_CTRL_A;
            else if (c == 5)                  ev.type = KEY_CTRL_E;
            else if (c >= 32 && c < 127)      ev.type = KEY_CHAR;
            else                              ev.type = KEY_NONE;

            if (ev.type != KEY_NONE) q_push(ev);
        }
    }

    outb(0x20, 0x20);
}

void keyboard_init(void) {
    q_head = q_tail = 0;
    shift_pressed = ctrl_pressed = alt_pressed = caps_lock = extended = 0;
}
