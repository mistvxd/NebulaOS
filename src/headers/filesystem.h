#pragma once
#include <stdint.h>

struct File {
    char name[16];
    char* data;
    int size;
};

extern struct File* files;
extern int file_count;

void create(char* name, char* data);
void create_str(char* name, char* str);
void create_bin(char* name, uint8_t* data, int size);
struct File* read_file(char* name);
struct File* read_bin(char* name);
void load_external_bin(char* name, int sector);
void delete_file(char* name);
void save_fs();
void load_fs();