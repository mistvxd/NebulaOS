#ifndef DISK_H
#define DISK_H

void read_sector(int lba, char* buffer);
void write_sector(int lba, char* buffer);

#endif