#ifndef FS_H
#define FS_H

#define MAX_FILES 12
#define MAX_FILENAME 32
#define MAX_FILE_SECTORS 16
#define FS_START_LBA 1000

#define FS_TYPE_FILE 0
#define FS_TYPE_DIR  1

#define FS_OK 0
#define FS_ERR_EXISTS -1
#define FS_ERR_NOFILE -2
#define FS_ERR_FULL -3
#define FS_ERR_TOOLARGE -4
#define FS_ERR_BADNAME -5
#define FS_ERR_TOOBIG_BUFFER -6
#define FS_ERR_DISK -7
#define FS_ERR_NOTDIR -8
#define FS_ERR_ISDIR -9
#define FS_ERR_NOTEMPTY -10

void fs_init(void);
int fs_create(const char *name);
int fs_write(const char *name, const unsigned char *data, unsigned int size);
int fs_read(const char *name, unsigned char *buffer, unsigned int buffer_capacity, unsigned int *size);
int fs_delete(const char *name);
void fs_list(void);
void fs_selftest(void);

int fs_mkdir(const char *name);
int fs_chdir(const char *name);
void fs_pwd(char *buffer, unsigned int capacity);

#endif
