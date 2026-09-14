#include "disk.h"

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned short inw(unsigned short port) {
    unsigned short ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static int ata_wait_bsy(void) {
    unsigned int timeout = 1000000;
    while (inb(0x1F7) & 0x80) {
        if (--timeout == 0) return -1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    unsigned int timeout = 1000000;
    while (!(inb(0x1F7) & 0x08)) {
        if (inb(0x1F7) & 0x01) return -1;
        if (--timeout == 0) return -1;
    }
    return 0;
}

int ata_read_sector(unsigned int lba, unsigned char *buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (unsigned char)lba);
    outb(0x1F4, (unsigned char)(lba >> 8));
    outb(0x1F5, (unsigned char)(lba >> 16));
    outb(0x1F7, 0x20);

    if (ata_wait_bsy() != 0) return -1;
    if (ata_wait_drq() != 0) return -1;

    for (int i = 0; i < 256; i++) {
        unsigned short data = inw(0x1F0);
        buffer[i * 2] = (unsigned char)data;
        buffer[i * 2 + 1] = (unsigned char)(data >> 8);
    }
    return 0;
}

int ata_write_sector(unsigned int lba, unsigned char *buffer) {
    if (ata_wait_bsy() != 0) return -1;

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (unsigned char)lba);
    outb(0x1F4, (unsigned char)(lba >> 8));
    outb(0x1F5, (unsigned char)(lba >> 16));
    outb(0x1F7, 0x30);

    if (ata_wait_bsy() != 0) return -1;
    if (ata_wait_drq() != 0) return -1;

    for (int i = 0; i < 256; i++) {
        unsigned short data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(0x1F0, data);
    }

    if (ata_wait_bsy() != 0) return -1;
    return 0;
}
