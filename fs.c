#include "fs.h"
#include "disk.h"
#include "screen.h"

struct file_entry {
    char name[MAX_FILENAME];
    unsigned int start_lba;
    unsigned int size;
    unsigned char used;
};

static struct file_entry files[MAX_FILES];

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

static void str_copy(char *dst, const char *src) {
    int i = 0;
    while (src[i] != '\0' && i < MAX_FILENAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int name_valid(const char *name) {
    int len = str_len(name);
    if (len == 0) return 0;
    if (len >= MAX_FILENAME) return 0;
    return 1;
}

void fs_init(void) {
    unsigned char buffer[512];
    int r = ata_read_sector(FS_START_LBA, buffer);
     if (r == 0 && buffer[0] == 'S' && buffer[1] == 'F' && buffer[2] == 'S' && buffer[3] == 1) {
        unsigned char *p = buffer + 4;
        for (int i = 0; i < MAX_FILES; i++) {
            files[i].used = p[0];
            for (int j = 0; j < MAX_FILENAME; j++) {
                files[i].name[j] = p[1 + j];
            }
            files[i].start_lba = p[1 + MAX_FILENAME] | (p[2 + MAX_FILENAME] << 8) |
                                  (p[3 + MAX_FILENAME] << 16) | (p[4 + MAX_FILENAME] << 24);
            files[i].size = p[5 + MAX_FILENAME] | (p[6 + MAX_FILENAME] << 8) |
                             (p[7 + MAX_FILENAME] << 16) | (p[8 + MAX_FILENAME] << 24);
            p += (1 + MAX_FILENAME + 8);
        }
    } else {
        for (int i = 0; i < MAX_FILES; i++) {
            files[i].used = 0;
        }
    }
}

static int fs_save_table(void) {
    unsigned char buffer[512];
    for (int i = 0; i < 512; i++) buffer[i] = 0;

    buffer[0] = 'S';
    buffer[1] = 'F';
    buffer[2] = 'S';
    buffer[3] = 1;

    unsigned char *p = buffer + 4;
    for (int i = 0; i < MAX_FILES; i++) {
        p[0] = files[i].used;
        for (int j = 0; j < MAX_FILENAME; j++) {
            p[1 + j] = files[i].name[j];
        }
        p[1 + MAX_FILENAME] = (unsigned char)(files[i].start_lba);
        p[2 + MAX_FILENAME] = (unsigned char)(files[i].start_lba >> 8);
        p[3 + MAX_FILENAME] = (unsigned char)(files[i].start_lba >> 16);
        p[4 + MAX_FILENAME] = (unsigned char)(files[i].start_lba >> 24);
        p[5 + MAX_FILENAME] = (unsigned char)(files[i].size);
        p[6 + MAX_FILENAME] = (unsigned char)(files[i].size >> 8);
        p[7 + MAX_FILENAME] = (unsigned char)(files[i].size >> 16);
        p[8 + MAX_FILENAME] = (unsigned char)(files[i].size >> 24);
        p += (1 + MAX_FILENAME + 8);
    }

 return ata_write_sector(FS_START_LBA, buffer);
}

int fs_create(const char *name) {
    if (!name_valid(name)) {
        return FS_ERR_BADNAME;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            return FS_ERR_EXISTS;
        }
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            str_copy(files[i].name, name);
            files[i].start_lba = FS_START_LBA + 1 + (i * MAX_FILE_SECTORS);
            files[i].size = 0;
            if (fs_save_table() != 0) {
                files[i].used = 0;
                return FS_ERR_DISK;
            }
            return FS_OK;       
        }
    }
    return FS_ERR_FULL;
}

int fs_write(const char *name, const unsigned char *data, unsigned int size) {
    if (!name_valid(name)) {
        return FS_ERR_BADNAME;    }

    unsigned int max_bytes = MAX_FILE_SECTORS * 512;
    if (size > max_bytes) {
        return FS_ERR_TOOLARGE;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            unsigned int sectors = size ? ((size + 511) / 512) : 0;
            unsigned char buffer[512];
            unsigned int written = 0;

            for (unsigned int s = 0; s < sectors; s++) {
                for (int j = 0; j < 512; j++) {
                    if (written < size) {
                        buffer[j] = data[written];
                        written++;
                    } else {
                        buffer[j] = 0;
                    }
                }
                if (ata_write_sector(files[i].start_lba + s, buffer) != 0) {
                    return FS_ERR_DISK;
                }
            }

            files[i].size = size;
            if (fs_save_table() != 0) {
                return FS_ERR_DISK;
            }
            return FS_OK;
        }
    }
    return FS_ERR_NOFILE;
}

int fs_read(const char *name, unsigned char *buffer, unsigned int buffer_capacity, unsigned int *size) {
    if (!name_valid(name)) {
        return FS_ERR_BADNAME;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            if (files[i].size > buffer_capacity) {
                return FS_ERR_TOOBIG_BUFFER;
            }

            unsigned int sectors = files[i].size ? ((files[i].size + 511) / 512) : 0;
            unsigned int read_count = 0;

            for (unsigned int s = 0; s < sectors; s++) {
                unsigned char sector_buf[512];
                if (ata_read_sector(files[i].start_lba + s, sector_buf) != 0) {
                    return FS_ERR_DISK;
                }
                for (int j = 0; j < 512; j++) {
                    if (read_count < files[i].size) {
                        buffer[read_count] = sector_buf[j];
                        read_count++;
                    }
                }
            }

            *size = files[i].size;
            return FS_OK;
        }
    }
    return FS_ERR_NOFILE;
}

int fs_delete(const char *name) {
    if (!name_valid(name)) {
        return FS_ERR_BADNAME;
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            files[i].used = 0;
            files[i].name[0] = '\0';
            files[i].size = 0;
            if (fs_save_table() != 0) {
                return FS_ERR_DISK;
            }
            return FS_OK;
        }
    }
    return FS_ERR_NOFILE;
}

void fs_list(void) {
    int any = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            print(files[i].name);
            print("\n");
            any = 1;
        }
    }
    if (!any) {
        print("(no files)\n");
    }
}

static void report(const char *test, int ok) {
    print(ok ? "[PASS] " : "[FAIL] ");
    print(test);
    print("\n");
}

void fs_selftest(void) {
    unsigned char buf[513];
    unsigned int size;
    int r;

    print("Running filesystem self-test...\n");

    r = fs_create("t1.txt");
    report("create t1.txt", r == FS_OK);

    r = fs_create("t1.txt");
    report("duplicate create rejected", r == FS_ERR_EXISTS);

    r = fs_write("t1.txt", (unsigned char*)"abc", 3);
    report("write t1.txt", r == FS_OK);

    r = fs_read("t1.txt", buf, 512, &size);
    buf[size] = 0;
    report("read t1.txt matches", r == FS_OK && size == 3 && str_eq((char*)buf, "abc"));

    r = fs_write("nofile.txt", (unsigned char*)"x", 1);
    report("write to missing file rejected", r == FS_ERR_NOFILE);

    r = fs_read("nofile.txt", buf, 512, &size);
    report("read missing file rejected", r == FS_ERR_NOFILE);

    r = fs_create("");
    report("empty name rejected", r == FS_ERR_BADNAME);

    r = fs_write("t1.txt", (unsigned char*)"x", 99999);
    report("oversize write rejected", r == FS_ERR_TOOLARGE);

    fs_create("t0.txt");
    r = fs_write("t0.txt", (unsigned char*)"", 0);
    report("zero-size write ok", r == FS_OK);

    r = fs_read("t0.txt", buf, 512, &size);
    report("zero-size read ok", r == FS_OK && size == 0);

    print("Self-test complete.\n");
}
