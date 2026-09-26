#include "fs.h"
#include "disk.h"
#include "screen.h"

#define FS_ROOT 255
#define FS_TABLE_SECTORS 2

struct file_entry {
    char name[MAX_FILENAME];
    unsigned int start_lba;
    unsigned int size;
    unsigned char used;
    unsigned char type;
    unsigned char parent;
};

static struct file_entry files[MAX_FILES];
static int cwd = -1;

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

static unsigned char cwd_byte(void) {
    return (cwd == -1) ? FS_ROOT : (unsigned char)cwd;
}

static int find_in_cwd(const char *name) {
    unsigned char parent = cwd_byte();
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && files[i].parent == parent && str_eq(files[i].name, name)) {
            return i;
        }
    }
    return -1;
}

void fs_init(void) {
    unsigned char buffer[1024];
    int ok = 1;
    for (int s = 0; s < FS_TABLE_SECTORS; s++) {
        if (ata_read_sector(FS_START_LBA + s, buffer + (s * 512)) != 0) {
            ok = 0;
        }
    }
    if (ok && buffer[0] == 'S' && buffer[1] == 'F' && buffer[2] == 'S' && buffer[3] == 2) {
        unsigned char *p = buffer + 4;
        for (int i = 0; i < MAX_FILES; i++) {
            files[i].used = p[0];
            files[i].type = p[1];
            files[i].parent = p[2];
            for (int j = 0; j < MAX_FILENAME; j++) {
                files[i].name[j] = p[3 + j];
            }
            unsigned char *q = p + 3 + MAX_FILENAME;
            files[i].start_lba = q[0] | (q[1] << 8) | (q[2] << 16) | (q[3] << 24);
            files[i].size = q[4] | (q[5] << 8) | (q[6] << 16) | (q[7] << 24);
            p += (3 + MAX_FILENAME + 8);
        }
    } else {
        for (int i = 0; i < MAX_FILES; i++) {
            files[i].used = 0;
        }
    }
    cwd = -1;
}

static int fs_save_table(void) {
    unsigned char buffer[1024];
    for (int i = 0; i < 1024; i++) buffer[i] = 0;
    buffer[0] = 'S';
    buffer[1] = 'F';
    buffer[2] = 'S';
    buffer[3] = 2;
    unsigned char *p = buffer + 4;
    for (int i = 0; i < MAX_FILES; i++) {
        p[0] = files[i].used;
        p[1] = files[i].type;
        p[2] = files[i].parent;
        for (int j = 0; j < MAX_FILENAME; j++) {
            p[3 + j] = files[i].name[j];
        }
        unsigned char *q = p + 3 + MAX_FILENAME;
        q[0] = (unsigned char)(files[i].start_lba);
        q[1] = (unsigned char)(files[i].start_lba >> 8);
        q[2] = (unsigned char)(files[i].start_lba >> 16);
        q[3] = (unsigned char)(files[i].start_lba >> 24);
        q[4] = (unsigned char)(files[i].size);
        q[5] = (unsigned char)(files[i].size >> 8);
        q[6] = (unsigned char)(files[i].size >> 16);
        q[7] = (unsigned char)(files[i].size >> 24);
        p += (3 + MAX_FILENAME + 8);
    }
    for (int s = 0; s < FS_TABLE_SECTORS; s++) {
        if (ata_write_sector(FS_START_LBA + s, buffer + (s * 512)) != 0) {
            return -1;
        }
    }
    return 0;
}

int fs_create(const char *name) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    if (find_in_cwd(name) != -1) return FS_ERR_EXISTS;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            files[i].type = FS_TYPE_FILE;
            files[i].parent = cwd_byte();
            str_copy(files[i].name, name);
            files[i].start_lba = FS_START_LBA + FS_TABLE_SECTORS + (i * MAX_FILE_SECTORS);
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

int fs_mkdir(const char *name) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    if (find_in_cwd(name) != -1) return FS_ERR_EXISTS;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            files[i].type = FS_TYPE_DIR;
            files[i].parent = cwd_byte();
            str_copy(files[i].name, name);
            files[i].start_lba = 0;
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

int fs_chdir(const char *name) {
    if (str_eq(name, ".")) return FS_OK;
    if (str_eq(name, "/")) { cwd = -1; return FS_OK; }
    if (str_eq(name, "..")) {
        if (cwd == -1) return FS_OK;
        unsigned char parent = files[cwd].parent;
        cwd = (parent == FS_ROOT) ? -1 : (int)parent;
        return FS_OK;
    }
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;
    if (files[idx].type != FS_TYPE_DIR) return FS_ERR_NOTDIR;
    cwd = idx;
    return FS_OK;
}

void fs_pwd(char *buffer, unsigned int capacity) {
    int chain[MAX_FILES];
    int depth = 0;
    int cur = cwd;
    while (cur != -1 && depth < MAX_FILES) {
        chain[depth] = cur;
        depth++;
        unsigned char parent = files[cur].parent;
        cur = (parent == FS_ROOT) ? -1 : (int)parent;
    }
    if (depth == 0) {
        if (capacity > 1) { buffer[0] = '/'; buffer[1] = '\0'; }
        return;
    }
    unsigned int pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos + 1 < capacity) buffer[pos++] = '/';
        const char *name = files[chain[i]].name;
        int j = 0;
        while (name[j] != '\0' && pos + 1 < capacity) {
            buffer[pos++] = name[j];
            j++;
        }
    }
    if (pos < capacity) buffer[pos] = '\0';
    else buffer[capacity - 1] = '\0';
}

int fs_write(const char *name, const unsigned char *data, unsigned int size) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    unsigned int max_bytes = MAX_FILE_SECTORS * 512;
    if (size > max_bytes) return FS_ERR_TOOLARGE;
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;
    if (files[idx].type != FS_TYPE_FILE) return FS_ERR_ISDIR;
    unsigned int sectors = size ? ((size + 511) / 512) : 0;
    unsigned char buffer[512];
    unsigned int written = 0;
    for (unsigned int s = 0; s < sectors; s++) {
        for (int j = 0; j < 512; j++) {
            if (written < size) { buffer[j] = data[written]; written++; }
            else buffer[j] = 0;
        }
        if (ata_write_sector(files[idx].start_lba + s, buffer) != 0) return FS_ERR_DISK;
    }
    files[idx].size = size;
    if (fs_save_table() != 0) return FS_ERR_DISK;
    return FS_OK;
}

int fs_read(const char *name, unsigned char *buffer, unsigned int buffer_capacity, unsigned int *size) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;
    if (files[idx].type != FS_TYPE_FILE) return FS_ERR_ISDIR;
    if (files[idx].size > buffer_capacity) return FS_ERR_TOOBIG_BUFFER;
    unsigned int sectors = files[idx].size ? ((files[idx].size + 511) / 512) : 0;
    unsigned int read_count = 0;
    for (unsigned int s = 0; s < sectors; s++) {
        unsigned char sector_buf[512];
        if (ata_read_sector(files[idx].start_lba + s, sector_buf) != 0) return FS_ERR_DISK;
        for (int j = 0; j < 512; j++) {
            if (read_count < files[idx].size) { buffer[read_count] = sector_buf[j]; read_count++; }
        }
    }
    *size = files[idx].size;
    return FS_OK;
}

int fs_delete(const char *name) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;
    if (files[idx].type == FS_TYPE_DIR) {
        unsigned char parent = (unsigned char)idx;
        for (int i = 0; i < MAX_FILES; i++) {
            if (files[i].used && files[i].parent == parent) return FS_ERR_NOTEMPTY;
        }
    }
    files[idx].used = 0;
    files[idx].name[0] = '\0';
    files[idx].size = 0;
    if (fs_save_table() != 0) return FS_ERR_DISK;
    return FS_OK;
}

int fs_move(const char *name, const char *dest) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;

    int destidx = find_in_cwd(dest);
if (destidx != -1 && files[destidx].type == FS_TYPE_DIR) {
    if (destidx == idx) return FS_ERR_CYCLE;

    if (files[idx].type == FS_TYPE_DIR) {
        int parent = destidx;

        for (int depth = 0; depth < MAX_FILES; depth++) {
            if (parent == idx) return FS_ERR_CYCLE;

            unsigned char next = files[parent].parent;
            if (next == FS_ROOT) break;
            if (next >= MAX_FILES || !files[next].used ||
                files[next].type != FS_TYPE_DIR) {
                return FS_ERR_CYCLE;
            }

            parent = (int)next;
            if (depth == MAX_FILES - 1) return FS_ERR_CYCLE;
        }
    }

    unsigned char destparent = (unsigned char)destidx;
        for (int i = 0; i < MAX_FILES; i++) {
            if (files[i].used && files[i].parent == destparent && str_eq(files[i].name, files[idx].name)) {
                return FS_ERR_EXISTS;
            }
        }
        files[idx].parent = destparent;
        if (fs_save_table() != 0) return FS_ERR_DISK;
        return FS_OK;
    }

    if (destidx != -1) return FS_ERR_EXISTS;
    if (!name_valid(dest)) return FS_ERR_BADNAME;
    str_copy(files[idx].name, dest);
    if (fs_save_table() != 0) return FS_ERR_DISK;
    return FS_OK;
}

int fs_copy(const char *name, const char *dest) {
    if (!name_valid(name)) return FS_ERR_BADNAME;
    int idx = find_in_cwd(name);
    if (idx == -1) return FS_ERR_NOFILE;
    if (files[idx].type != FS_TYPE_FILE) return FS_ERR_ISDIR;

    unsigned char target_parent = cwd_byte();
    char target_name[MAX_FILENAME];
    str_copy(target_name, files[idx].name);

    int destidx = find_in_cwd(dest);
    if (destidx != -1 && files[destidx].type == FS_TYPE_DIR) {
        target_parent = (unsigned char)destidx;
    } else {
        if (!name_valid(dest)) return FS_ERR_BADNAME;
        str_copy(target_name, dest);
    }

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && files[i].parent == target_parent && str_eq(files[i].name, target_name)) {
            return FS_ERR_EXISTS;
        }
    }

    int newidx = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) { newidx = i; break; }
    }
    if (newidx == -1) return FS_ERR_FULL;

    files[newidx].used = 1;
    files[newidx].type = FS_TYPE_FILE;
    files[newidx].parent = target_parent;
    str_copy(files[newidx].name, target_name);
    files[newidx].start_lba = FS_START_LBA + FS_TABLE_SECTORS + (newidx * MAX_FILE_SECTORS);
    files[newidx].size = 0;

    unsigned int size = files[idx].size;
    unsigned int sectors = size ? ((size + 511) / 512) : 0;
    unsigned char buf[512];
    for (unsigned int s = 0; s < sectors; s++) {
        if (ata_read_sector(files[idx].start_lba + s, buf) != 0) {
            files[newidx].used = 0;
            return FS_ERR_DISK;
        }
        if (ata_write_sector(files[newidx].start_lba + s, buf) != 0) {
            files[newidx].used = 0;
            return FS_ERR_DISK;
        }
    }
    files[newidx].size = size;
    if (fs_save_table() != 0) {
        files[newidx].used = 0;
        return FS_ERR_DISK;
    }
    return FS_OK;
}

int fs_complete(const char *prefix, char *out, unsigned int out_cap) {
    unsigned char parent = cwd_byte();
    int plen = str_len(prefix);
    int count = 0, first = -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) continue;
        if (files[i].parent != parent) continue;
        int ok = 1;
        for (int j = 0; j < plen; j++) {
            if (files[i].name[j] == '\0' || files[i].name[j] != prefix[j]) {
                ok = 0; break;
            }
        }
        if (!ok) continue;
        count++;
        if (first < 0) first = i;
    }

    if (count == 0) return -1;
    if (count == 1) {
        const char *nm = files[first].name;
        unsigned int k = 0;
        while (nm[k] && k + 1 < out_cap) { out[k] = nm[k]; k++; }
        if (files[first].type == FS_TYPE_DIR && k + 1 < out_cap) {
            out[k++] = '/';
        }
        out[k] = '\0';
        return 0;
    }
    return count;
}

void fs_list_prefix(const char *prefix) {
    unsigned char parent = cwd_byte();
    int plen = str_len(prefix);
    int any = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) continue;
        if (files[i].parent != parent) continue;
        int ok = 1;
        for (int j = 0; j < plen; j++) {
            if (files[i].name[j] == '\0' || files[i].name[j] != prefix[j]) {
                ok = 0; break;
            }
        }
        if (!ok) continue;
        print(files[i].name);
        if (files[i].type == FS_TYPE_DIR) print("/");
        print("  ");
        any = 1;
    }
    if (any) print("\n");
}

void fs_list(void) {
    int any = 0;
    unsigned char parent = cwd_byte();
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && files[i].parent == parent) {
            print(files[i].name);
            if (files[i].type == FS_TYPE_DIR) print("/");
            print("\n");
            any = 1;
        }
    }
    if (!any) print("(empty)\n");
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

    r = fs_mkdir("sub");
    report("mkdir sub", r == FS_OK);

    r = fs_mkdir("sub");
    report("duplicate mkdir rejected", r == FS_ERR_EXISTS);

    r = fs_chdir("sub");
    report("chdir into sub", r == FS_OK);

    r = fs_create("inside.txt");
    report("create file inside sub", r == FS_OK);

    r = fs_read("t1.txt", buf, 512, &size);
    report("root file hidden from sub", r == FS_ERR_NOFILE);

    r = fs_chdir("..");
    report("chdir back to root", r == FS_OK);

    r = fs_read("t1.txt", buf, 512, &size);
    report("t1.txt visible again at root", r == FS_OK);

    r = fs_delete("sub");
    report("rmdir non-empty rejected", r == FS_ERR_NOTEMPTY);

    r = fs_move("t1.txt", "moved.txt");
    report("mv rename ok", r == FS_OK);

    r = fs_copy("moved.txt", "copy.txt");
    report("cp file ok", r == FS_OK);

    r = fs_read("copy.txt", buf, 512, &size);
    buf[size] = 0;
    report("copy content matches", r == FS_OK && str_eq((char*)buf, "abc"));

    print("Self-test complete.\n");
}
