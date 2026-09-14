#include "shell.h"
#include "screen.h"
#include "fs.h"

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
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

static void do_reboot(void) {
    unsigned char temp;
    do {
        temp = inb(0x64);
        if (temp & 1) inb(0x60);
    } while (temp & 2);
    outb(0x64, 0xFE);
    while (1) {
        __asm__ volatile ("hlt");
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
    print("Error: filesystem error.\n");
}

void shell_prompt(void) {
    print("\nsimorgh> ");
    set_input_boundary();
}

void shell_init(void) {
    fs_init();
    print("\nType 'help' for a list of commands.\n");
    shell_prompt();
}

void shell_execute(char *line) {
    print("\n");

    if (str_len(line) == 0) {
        shell_prompt();
        return;
    }

    if (str_eq(line, "help")) {
        print("Commands:\n");
        print("  help              - show this list\n");
        print("  info              - show OS info\n");
        print("  clear             - clear the screen\n");
        print("  echo <text>       - print text\n");
        print("  ls                - list files\n");
        print("  cat <file>        - show file content\n");
        print("  write <file> <t>  - create/overwrite file with text\n");
        print("  rm <file>         - delete a file\n");
        print("  fstest            - run filesystem self-test\n");
        print("  reboot            - restart the system\n");
        shell_prompt();
        return;
    }

    if (str_eq(line, "info")) {
        print("Simorgh OS\n");
        print("Version 0.2\n");
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

    if (str_eq(line, "cat") || str_eq(line, "write") || str_eq(line, "rm")) {
        print("Error: invalid argument.\n");
        shell_prompt();
        return;
    }

    if (starts_with(line, "cat ")) {
        char *filename = line + 4;
        unsigned char buf[513];
        unsigned int size;
        int r = fs_read(filename, buf, 512, &size);
        if (r == FS_OK) {
            buf[size] = 0;
            print((char*)buf);
            print("\n");
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
            print("File deleted.\n");
        } else {
            print_fs_error(r);
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
