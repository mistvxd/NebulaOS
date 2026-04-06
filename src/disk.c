#include "headers/disk.h"

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void ata_wait() {
    while (inb(0x1F7) & 0x80);
}

void read_sector(int lba, char* buffer) {
    ata_wait();

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x20);

    ata_wait();

    for (int i = 0; i < 256; i++) {
        unsigned short data;
        asm volatile("inw %1, %0" : "=a"(data) : "Nd"(0x1F0));
        ((unsigned short*)buffer)[i] = data;
    }
}

void write_sector(int lba, char* buffer) {
    ata_wait();

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F7, 0x30);

    ata_wait();

    for (int i = 0; i < 256; i++) {
        asm volatile("outw %0, %1" : : "a"(((unsigned short*)buffer)[i]), "Nd"(0x1F0));
    }
}