#include "shell.h"
#include "screen.h"
#include "fs.h"

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int str_len(const char *s) {
    int i = 0;
    while (s[i] != '\0') i++;
    return i;
}

static int starts_with(const char *s, const char *prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (s[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

#define LINE_MAX     127
#define HISTORY_MAX  16

static char line_buf[LINE_MAX + 1];
static int  line_len         = 0;
static int  line_cursor      = 0;
static int  input_start_row  = 0;
static int  input_start_col  = 0;
static int  input_max_used   = 0;

static char history[HISTORY_MAX][LINE_MAX + 1];
static int  history_count = 0;
static int  history_view  = -1;
static char history_saved[LINE_MAX + 1];

static void mem_zero(char *p, int n) {
    for (int i = 0; i < n; i++) p[i] = 0;
}

static void redraw_line(void) {
    screen_set_cursor(input_start_row, input_start_col);

    for (int i = 0; i < line_len; i++) putchar(line_buf[i]);

    for (int i = line_len; i < input_max_used; i++) putchar(' ');

    int abs = input_start_col + line_cursor;
    screen_set_cursor(input_start_row + abs / 80, abs % 80);
}

static void input_insert(char c) {
    if (line_len >= LINE_MAX) return;

    for (int i = line_len; i > line_cursor; i--) {
        line_buf[i] = line_buf[i - 1];
    }
    line_buf[line_cursor] = c;
    line_len++;
    line_cursor++;

    if (line_cursor == line_len) {
        putchar(c);
        if (line_len > input_max_used) input_max_used = line_len;
    } else {
        if (line_len > input_max_used) input_max_used = line_len;
        redraw_line();
    }
}

static void input_backspace(void) {
    if (line_cursor == 0) return;

    for (int i = line_cursor - 1; i < line_len - 1; i++) {
        line_buf[i] = line_buf[i + 1];
    }
    line_len--;
    line_cursor--;

    if (line_cursor == line_len) {
        putchar('\b');
        if (input_max_used > line_len) {
            putchar(' ');
            putchar('\b');
            input_max_used = line_len;
        }
    } else {
        redraw_line();
    }
}

static void input_delete(void) {
    if (line_cursor >= line_len) return;
    for (int i = line_cursor; i < line_len - 1; i++) {
        line_buf[i] = line_buf[i + 1];
    }
    line_len--;
    redraw_line();
}

static void history_apply_view(void) {
    const char *src;
    if (history_view == -1) src = history_saved;
    else                    src = history[history_count - 1 - history_view];

    int i = 0;
    while (src[i] && i < LINE_MAX) { line_buf[i] = src[i]; i++; }
    line_buf[i] = '\0';
    line_len    = i;
    line_cursor = i;

    if (line_len > input_max_used) input_max_used = line_len;
    redraw_line();
}

static void history_up(void) {
if (history_count == 0) return;
    if (history_view == -1) {
        int i = 0;
        while (line_buf[i] && i < LINE_MAX) { history_saved[i] = line_buf[i]; i++; }
        history_saved[i] = '\0';
    }
    int max_view = history_count - 1;
    if (history_view < max_view) {
        history_view++;
        history_apply_view();
    }
}

static void history_down(void) {
    if (history_view > -1) {
        history_view--;
        history_apply_view();
    }
}

static void history_add(const char *line) {
    if (line[0] == '\0') return;

    if (history_count > 0) {
        int last = history_count - 1;
        if (str_eq(history[last], line)) return;
    }

    if (history_count < HISTORY_MAX) {
        int i = 0;
        while (line[i] && i < LINE_MAX) { history[history_count][i] = line[i]; i++; }
        history[history_count][i] = '\0';
        history_count++;
    } else {
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            int j = 0;
            while (history[i + 1][j] && j < LINE_MAX) {
                history[i][j] = history[i + 1][j]; j++;
            }
            history[i][j] = '\0';
        }
        int i = 0;
        while (line[i] && i < LINE_MAX) { history[HISTORY_MAX - 1][i] = line[i]; i++; }
        history[HISTORY_MAX - 1][i] = '\0';
    }
}

static void tab_complete(void) {
    if (line_cursor != line_len) return;

    int start = line_cursor;
    while (start > 0 && line_buf[start - 1] != ' ') start--;
    int prefix_len = line_cursor - start;
    if (prefix_len == 0) return;

    char prefix[MAX_FILENAME];
    int n = prefix_len < MAX_FILENAME - 1 ? prefix_len : MAX_FILENAME - 1;
    for (int i = 0; i < n; i++) prefix[i] = line_buf[start + i];
    prefix[n] = '\0';

    char match[MAX_FILENAME + 2];
    int r = fs_complete(prefix, match, sizeof(match));

    if (r < 0) return;
    if (r == 0) {
        int mlen = str_len(match);

        line_len -= prefix_len;
        for (int i = 0; i < mlen && line_len < LINE_MAX; i++) {
            line_buf[line_len++] = match[i];
        }
        if (mlen > 0 && match[mlen - 1] != '/' && line_len < LINE_MAX) {
            line_buf[line_len++] = ' ';
        }
        line_buf[line_len] = '\0';
        line_cursor = line_len;
        if (line_len > input_max_used) input_max_used = line_len;
        redraw_line();
    } else {
        print("\n");
        fs_list_prefix(prefix);
        shell_prompt();
        for (int i = 0; i < line_len; i++) putchar(line_buf[i]);
        input_max_used = line_len;
        int abs = input_start_col + line_cursor;
        screen_set_cursor(input_start_row + abs / 80, abs % 80);
    }
}

void shell_handle_event(key_event_t *ev) {

    if (screen_is_scrolled()) {
        switch (ev->type) {
            case KEY_UP:
            case KEY_PAGE_UP:
                screen_scroll(+5);
                return;

            case KEY_DOWN:
            case KEY_PAGE_DOWN:
                screen_scroll(-5);
                return;

            case KEY_HOME:
                screen_scroll_top();
                return;

            case KEY_END:
                screen_scroll_bottom();
                return;

            default:
                screen_scroll_bottom();
                break;
        }
    }

    if (ev->type == KEY_PAGE_UP) {
        screen_scroll(+5);
        return;
    }

    if (ev->type == KEY_UP && (ev->mods & 1)) {
        screen_scroll(+5);
        return;
    }
    if (ev->type == KEY_DOWN && (ev->mods & 1)) {
        screen_scroll(-5);
        return;
    }

    switch (ev->type) {
        case KEY_CHAR:
            input_insert(ev->ch);
            break;

        case KEY_ENTER:
            line_buf[line_len] = '\0';
            putchar('\n');
            history_add(line_buf);
            history_view = -1;
            input_max_used = 0;
            {
                char cmd[LINE_MAX + 1];
                int i = 0;
                while (line_buf[i] && i < LINE_MAX) { cmd[i] = line_buf[i]; i++; }
                cmd[i] = '\0';

                line_len = 0;
                line_cursor = 0;
                mem_zero(line_buf, LINE_MAX + 1);

                shell_execute(cmd);
            }
            break;

        case KEY_BACKSPACE: input_backspace(); break;
        case KEY_DELETE:    input_delete();    break;

        case KEY_LEFT:
            if (line_cursor > 0) {
                line_cursor--;
                int abs = input_start_col + line_cursor;
                screen_set_cursor(input_start_row + abs / 80, abs % 80);
            }
            break;

        case KEY_RIGHT:
            if (line_cursor < line_len) {
                line_cursor++;
                int abs = input_start_col + line_cursor;
                screen_set_cursor(input_start_row + abs / 80, abs % 80);
            }
            break;

        case KEY_HOME:
            line_cursor = 0;
            screen_set_cursor(input_start_row, input_start_col);
            break;

        case KEY_END:
            line_cursor = line_len;
            {
                int abs = input_start_col + line_cursor;
                screen_set_cursor(input_start_row + abs / 80, abs % 80);
            }
            break;

        case KEY_UP:   history_up();   break;
        case KEY_DOWN: history_down(); break;
        case KEY_TAB:  tab_complete(); break;

        case KEY_CTRL_L:
            clear_screen();
            shell_prompt();
            for (int i = 0; i < line_len; i++) putchar(line_buf[i]);
            input_max_used = line_len;
            {
                int abs = input_start_col + line_cursor;
                screen_set_cursor(input_start_row + abs / 80, abs % 80);
            }
            break;

        case KEY_CTRL_C:
            print("^C\n");
            line_len = 0;
            line_cursor = 0;
            history_view = -1;
            input_max_used = 0;
            mem_zero(line_buf, LINE_MAX + 1);
            shell_prompt();
            break;

        case KEY_CTRL_U:
            line_len = 0;
            line_cursor = 0;
            redraw_line();
            break;

        case KEY_CTRL_W: {
            int end = line_cursor;
            while (line_cursor > 0 && line_buf[line_cursor - 1] == ' ') line_cursor--;
            while (line_cursor > 0 && line_buf[line_cursor - 1] != ' ') line_cursor--;
            int removed = end - line_cursor;
            for (int i = line_cursor; i < line_len - removed; i++) {
                line_buf[i] = line_buf[i + removed];
            }
            line_len -= removed;
            redraw_line();
            break;
        }

        default: break;
    }
}
static void do_reboot(void) {
    unsigned char temp;
    do {
        temp = inb(0x64);
        if (temp & 1) inb(0x60);
    } while (temp & 2);
    outb(0x64, 0xFE);
    while (1) {
        asm volatile ("hlt");
    }
}

static void print_fs_error(int code) {
    if (code == FS_ERR_EXISTS) { print("Error: file already exists.\n"); return; }
    if (code == FS_ERR_NOFILE) { print("Error: file not found.\n"); return; }
    if (code == FS_ERR_FULL) { print("Error: filesystem is full.\n"); return; }
    if (code == FS_ERR_TOOLARGE) { print("Error: file too large.\n"); return; }
    if (code == FS_ERR_BADNAME) { print("Error: invalid filename.\n"); return; }
    if (code == FS_ERR_TOOBIG_BUFFER) { print("Error: file too large for buffer.\n"); return; }
    if (code == FS_ERR_DISK) { print("Error: disk error.\n"); return; }
    if (code == FS_ERR_NOTDIR) { print("Error: not a directory.\n"); return; }
    if (code == FS_ERR_ISDIR) { print("Error: is a directory.\n"); return; }
    if (code == FS_ERR_NOTEMPTY) { print("Error: directory not empty.\n"); return; }
    print("Error: filesystem error.\n");
}

void shell_prompt(void) {
    char path[128];
    fs_pwd(path, sizeof(path));
    print("simorgh:");
    print(path);
    print("> ");
    set_input_boundary();
    screen_get_cursor(&input_start_row, &input_start_col);
    input_max_used = 0;
}

void shell_init(void) {
    fs_init();
    line_len       = 0;
    line_cursor    = 0;
    history_count  = 0;
    history_view   = -1;
    input_max_used = 0;
    mem_zero(line_buf, LINE_MAX + 1);
print("\nType 'help' for a list of commands.\n");
    shell_prompt();
}

void shell_execute(char *line) {
    if (str_len(line) == 0) {
        shell_prompt();
        return;
    }

    if (str_eq(line, "help")) {
        print("Commands:\n");
        print("  help            - show this list\n");
        print("  info            - show OS info\n");
        print("  clear           - clear the screen\n");
        print("  echo <text>     - print text\n");
        print("  ls              - list files/dirs\n");
        print("  cat <file>      - show file content\n");
        print("  write <file> <t>- create/overwrite file with text\n");
        print("  rm <name>       - delete a file or empty dir\n");
        print("  mkdir <name>    - create a directory\n");
        print("  cd <name>       - change directory (.. or / supported)\n");
        print("  pwd             - print current directory\n");
        print("  mv <src> <dst>  - rename, or move into a directory\n");
        print("  cp <src> <dst>  - copy a file\n");
        print("  fstest          - run filesystem self-test\n");
        print("  reboot          - restart the system\n");
        print("\nKey bindings:\n");
        print("  Up/Down         - command history\n");
        print(" Shift+Up/Down    - scroll through output");
        print("  Left/Right      - move cursor\n");
        print("  Home/End        - jump to start/end\n");
        print("  Tab             - autocomplete filename\n");
        print("  Ctrl+L          - clear screen\n");
        print("  Ctrl+C          - cancel current line\n");
        print("  Ctrl+U          - clear whole line\n");
        print("  Ctrl+W          - delete last word\n");
        shell_prompt();
        return;
    }

    if (str_eq(line, "info")) {
        print("Simorgh OS\n");
        print("Version 0.4.1\n");
        print("Architecture: x86\n");
        print("Kernel: Simorgh Kernel\n");
        print("Filesystem: SimorghFS\n");
        print("Shell: Simorgh Shell\n");
        shell_prompt();
        return;
    }

    if (str_eq(line, "clear")) {
        clear_screen();
        shell_prompt();
        return;
    }

    if (starts_with(line, "echo ")) {
        print(line + 5);
        print("\n");
        shell_prompt();
        return;
    }

    if (str_eq(line, "ls")) {
        fs_list();
        shell_prompt();
        return;
    }

    if (str_eq(line, "pwd")) {
        char path[128];
        fs_pwd(path, sizeof(path));
        print(path);
        print("\n");
        shell_prompt();
        return;
    }

if (str_eq(line, "cat") || str_eq(line, "write") || str_eq(line, "rm") ||
    str_eq(line, "mkdir") || str_eq(line, "cd") ||
    str_eq(line, "mv") || str_eq(line, "cp")) {
        print("Error: invalid argument.\n");
        shell_prompt();
        return;
    }

    if (starts_with(line, "cat ")) {
        char *filename = line + 4;
        static unsigned char buf[MAX_FILE_SECTORS * 512 + 1];
        unsigned int size;
        int r = fs_read(filename, buf, MAX_FILE_SECTORS * 512, &size);
        if (r == FS_OK) {
            buf[size] = 0;
            print((char*)buf);
            if (size == 0 || buf[size - 1] != '\n') print("\n");
        } else {
            print_fs_error(r);
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "write ")) {
        char *rest = line + 6;
        int i = 0;
        while (rest[i] != '\0' && rest[i] != ' ') i++;
        if (rest[i] == '\0') {
            print("Error: invalid argument.\n");
        } else {
            rest[i] = '\0';
            char *filename = rest;
            char *content = rest + i + 1;
            int r = fs_create(filename);
if (r != FS_OK && r != FS_ERR_EXISTS) {
                print_fs_error(r);
            } else {
                r = fs_write(filename, (unsigned char*)content, str_len(content));
                if (r == FS_OK) {
                    print("File written.\n");
                } else {
                    print_fs_error(r);
                }
            }
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "rm ")) {
        char *filename = line + 3;
        int r = fs_delete(filename);
        if (r == FS_OK) {
            print("Deleted.\n");
        } else {
            print_fs_error(r);
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "mkdir ")) {
        char *dirname = line + 6;
        int r = fs_mkdir(dirname);
        if (r == FS_OK) {
            print("Directory created.\n");
        } else {
            print_fs_error(r);
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "cd ")) {
        char *dirname = line + 3;
        int r = fs_chdir(dirname);
        if (r != FS_OK) {
            print_fs_error(r);
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "mv ")) {
        char *rest = line + 3;
        int i = 0;
        while (rest[i] != '\0' && rest[i] != ' ') i++;
        if (rest[i] == '\0') {
            print("Error: invalid argument.\n");
        } else {
            rest[i] = '\0';
            char *src = rest;
            char *dst = rest + i + 1;
            int r = fs_move(src, dst);
            if (r == FS_OK) {
                print("Moved.\n");
            } else {
                print_fs_error(r);
            }
        }
        shell_prompt();
        return;
    }

    if (starts_with(line, "cp ")) {
        char *rest = line + 3;
        int i = 0;
        while (rest[i] != '\0' && rest[i] != ' ') i++;
        if (rest[i] == '\0') {
            print("Error: invalid argument.\n");
        } else {
            rest[i] = '\0';
            char *src = rest;
            char *dst = rest + i + 1;
            int r = fs_copy(src, dst);
            if (r == FS_OK) {
                print("Copied.\n");
            } else {
                print_fs_error(r);
            }
        }
        shell_prompt();
        return;
    }

    if (str_eq(line, "fstest")) {
        fs_selftest();
        shell_prompt();
        return;
    }

    if (str_eq(line, "reboot")) {
        print("Rebooting...\n");
        do_reboot();
        return;
    }

    print("Unknown command: ");
    print(line);
    print("\n");
    shell_prompt();
}
