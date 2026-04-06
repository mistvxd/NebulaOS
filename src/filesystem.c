#include "headers/filesystem.h"
#include "headers/disk.h"
#include "headers/heap.h"
#include <stdbool.h>
#include <stdint.h>

#define FS_SECTORS 4
#define MAX_FILES 64

struct File* files = 0;
int file_count = 0;
bool debug_mode = false;

// ================= utils =================

static int str_eq(char* a, char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int str_len(char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

void debug_print(char* s) {
    if (!debug_mode) return;
    extern void vga_print(const char*);
    vga_print(s);
}

void debug_print_num(int n) {
    extern void itoa(int, char*);
    char buf[16];
    itoa(n, buf);
    debug_print(buf);
}

extern void itoa(int num, char* str);

void debug_dump_mem(uint8_t* ptr, int size) {
    for (int i = 0; i < size; i++) {
        char c = ptr[i];
        if (c >= 32 && c <= 126)
            debug_print((char[]){c,0});
        else
            debug_print(".");
    }
}

// ================= create =================

void create(char* name, char* data) {
    int size = str_len(data) + 1;

    // overwrite
    for (int i = 0; i < file_count; i++) {
        if (str_eq(files[i].name, name)) {
            files[i].data = malloc(size);
            files[i].size = size;

            for (int j = 0; j < size; j++)
                files[i].data[j] = data[j];

            return;
        }
    }

    if (file_count >= MAX_FILES)
        return;

    struct File* new_files = malloc(sizeof(struct File) * (file_count + 1));

    for (int i = 0; i < file_count; i++) {
        new_files[i] = files[i];

        char* new_data = malloc(files[i].size);
        for (int j = 0; j < files[i].size; j++)
            new_data[j] = files[i].data[j];

        new_files[i].data = new_data;
    }

    files = new_files;

    struct File* f = &files[file_count];

    for (int j = 0; j < 16; j++)
        f->name[j] = 0;

    // nome fixo
    for (int i = 0; i < 16; i++)
        f->name[i] = 0;

    int i = 0;
    while (name[i] && i < 15) {
        f->name[i] = name[i];
        i++;
    }

    // dados
    f->data = malloc(size);
    f->size = size;

    char buf[16];

    debug_print("ALLOC PTR: ");
    itoa((int)f->data, buf);
    debug_print(buf);
    debug_print("\n");

    for (int j = 0; j < size; j++)
        f->data[j] = data[j];

    file_count++;
}

void create_bin(char* name, uint8_t* data, int size) {

    // ===== overwrite =====
    for (int i = 0; i < file_count; i++) {

        if ((uint8_t)files[i].name[0] != 0xFF)
            continue;

        char* real = &files[i].name[1];

        if (str_eq(real, name)) {

            files[i].data = malloc(size);
            files[i].size = size;

            for (int j = 0; j < size; j++)
                files[i].data[j] = data[j];

            return;
        }
    }

    // ===== create novo =====
    if (file_count >= MAX_FILES)
        return;

    struct File* new_files = malloc(sizeof(struct File) * (file_count + 1));

    for (int i = 0; i < file_count; i++)
        new_files[i] = files[i];

    files = new_files;

    struct File* f = &files[file_count];

    // nome
    f->name[0] = (char)0xFF;

    int i = 0;
    while (name[i] && i < 14) {
        f->name[i + 1] = name[i];
        i++;
    }
    f->name[i + 1] = 0;

    // dados
    f->data = malloc(size);
    f->size = size;

    for (int j = 0; j < size; j++)
        f->data[j] = data[j];

    file_count++;
}

// ================= read =================

struct File* read_file(char* name) {
    for (int i = 0; i < file_count; i++) {
        if (str_eq(files[i].name, name))
            return &files[i];
    }
    return 0;
}

struct File* read_bin(char* name) {
    for (int i = 0; i < file_count; i++) {

        // só binários
        if ((uint8_t)files[i].name[0] != 0xFF)
            continue;

        // compara ignorando o 0xFF
        if (str_eq(&files[i].name[1], name))
            return &files[i];
    }

    return 0;
}

// ================= delete =================

void delete_file(char* name) {
    for (int i = 0; i < file_count; i++) {
        if (str_eq(files[i].name, name)) {

            for (int j = i; j < file_count - 1; j++)
                files[j] = files[j + 1];

            file_count--;
            return;
        }
    }
}

void load_external_bin(char* name, int sector) {
    uint8_t buffer[512];
    read_sector(sector, buffer);

    int size = *(int*)buffer;

    uint8_t* data = malloc(size);

    // copia corretamente (inclusive múltiplos setores)
    int read = 0;
    int s = sector;

    while (read < size) {
        read_sector(s, buffer);

        int start = (s == sector) ? 4 : 0;

        for (int i = start; i < 512 && read < size; i++) {
            data[read++] = buffer[i];
        }
    }

    char buf[16];
    extern void vga_print(const char*);
    vga_print("DATA[0]: ");
    itoa((int)data[0], buf);
    vga_print(buf);
    vga_print("\n");

    s++;
    create_bin(name, data, size);
}


// ================= save =================

void save_fs() {
    static uint8_t buffer[512 * FS_SECTORS];

    // limpa buffer
    for (int i = 0; i < sizeof(buffer); i++)
        buffer[i] = 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + sizeof(buffer);

    // escreve quantidade
    *(int*)ptr = file_count;
    ptr += 4;

    for (int i = 0; i < file_count; i++) {

        // nome (16 bytes fixo)
        for (int j = 0; j < 16; j++)
            *ptr++ = files[i].name[j];

        // BINÁRIO
        if ((uint8_t)files[i].name[0] == 0xFF) {

            // escreve tamanho
            if (ptr + 4 >= end) return;
            *(int*)ptr = files[i].size;
            ptr += 4;

            // escreve dados
            if (ptr + files[i].size >= end) return;
            for (int j = 0; j < files[i].size; j++)
                *ptr++ = files[i].data[j];
        }
        // STRING (modo antigo)
        else {
            int j = 0;
            while (1) {
                if (ptr >= end) return;

                char c = files[i].data[j++];
                *ptr++ = c;

                if (c == 0)
                    break;
            }
        }
    }

    // escreve no disco
    for (int i = 0; i < FS_SECTORS; i++)
        write_sector(1 + i, buffer + i * 512);
}

// ================= load =================

void load_fs() {
    static uint8_t buffer[512 * FS_SECTORS];

    for (int i = 0; i < FS_SECTORS; i++)
        read_sector(1 + i, buffer + i * 512);

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + sizeof(buffer);

    // lê quantidade
    file_count = *(int*)ptr;
    ptr += 4;

    if (file_count < 0 || file_count > MAX_FILES)
        file_count = 0;

    files = malloc(sizeof(struct File) * file_count);

    for (int i = 0; i < file_count; i++) {

        // nome
        if (ptr + 16 > end) break;

        for (int j = 0; j < 16; j++)
            files[i].name[j] = *ptr++;

        files[i].name[15] = 0;

        // BINÁRIO
        if ((uint8_t)files[i].name[0] == 0xFF) {

            if (ptr + 4 > end) break;

            int size = *(int*)ptr;
            ptr += 4;

            if (size <= 0 || ptr + size > end)
                break;

            files[i].data = malloc(size);
            files[i].size = size;

            for (int j = 0; j < size; j++)
                files[i].data[j] = *ptr++;
        }
        // STRING
        else {
            char temp[256];
            int size = 0;

            while (ptr < end && size < 255) {
                char c = *ptr++;
                temp[size++] = c;

                if (c == 0)
                    break;
            }

            // garante terminador
            if (size == 0 || temp[size - 1] != 0) {
                temp[size++] = 0;
            }

            files[i].data = malloc(size);
            files[i].size = size;

            for (int j = 0; j < size; j++)
                files[i].data[j] = temp[j];
        }
    }
}