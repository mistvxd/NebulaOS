#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#define FS_KIND_FILE 0
#define FS_KIND_BIN 1
#define FS_KIND_DIR 2

typedef struct File {
    char name[16];
    uint8_t* data;
    int size;
    uint8_t kind;
    uint8_t alive;
    uint8_t parent;
    uint8_t reserved;
} File;

extern File* files;
extern int file_count;
extern int current_dir;
extern bool debug_mode;

void debug_print(char* s);
void debug_print_num(int n);
void debug_dump_mem(uint8_t* ptr, int size);

void create(char* name, char* data);
void create_bin(char* name, uint8_t* data, int size);
struct File* read_file(char* name);
struct File* read_bin(char* name);
void delete_file(char* name);
void load_external_bin(char* name, int sector);
int resolve_dir_path(char* path, int create_missing);
int fs_open(char* path, int mode);

int mkdir_path(char* path);
int cd_path(char* path);
void fs_reset(void);

void save_fs(void);
void load_fs(void);

#endif