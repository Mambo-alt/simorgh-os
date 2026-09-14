#ifndef DISK_H
#define DISK_H

int ata_read_sector(unsigned int lba, unsigned char *buffer);
int ata_write_sector(unsigned int lba, unsigned char *buffer);

#endif
