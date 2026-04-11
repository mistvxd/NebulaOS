#include "headers/filesystem.h"
#include "headers/disk.h"
#include "headers/heap.h"
#include <stdbool.h>
#include <stdint.h>

#define FS_SECTORS 4
#define MAX_FILES 64
#define FS_MAGIC 0x32465352

static File file_storage[MAX_FILES];
File* files = file_storage;
int file_count = 0;
int current_dir = 0;
bool debug_mode = false;

extern void vga_print(const char*);

extern void itoa(int num, char* str);

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

static void str_copy16(char* dst, const char* src) {
    for (int i = 0; i < 16; i++) dst[i] = 0;
    int i = 0;
    while (src[i] && i < 15) {
        dst[i] = src[i];
        i++;
    }
}

void debug_print(char* s) {
    if (!debug_mode) return;
    vga_print(s);
}

void debug_print_num(int n) {
    char buf[16];
    itoa(n, buf);
    debug_print(buf);
}

void debug_dump_mem(uint8_t* ptr, int size) {
    for (int i = 0; i < size; i++) {
        char c = ptr[i];
        if (c >= 32 && c <= 126)
            debug_print((char[]){c, 0});
        else
            debug_print(".");
    }
}

static void clear_slot(int idx) {
    for (int i = 0; i < 16; i++) files[idx].name[i] = 0;
    files[idx].data = 0;
    files[idx].size = 0;
    files[idx].kind = 0;
    files[idx].alive = 0;
    files[idx].parent = 0;
    files[idx].reserved = 0;
}

static void fs_init_root(void) {
    if (file_count > 0 && files[0].alive && files[0].kind == FS_KIND_DIR) return;

    for (int i = 0; i < MAX_FILES; i++) clear_slot(i);

    file_count = 1;
    current_dir = 0;
    str_copy16(files[0].name, "/");
    files[0].kind = FS_KIND_DIR;
    files[0].alive = 1;
    files[0].parent = 0;
}

void fs_reset(void) {
    for (int i = 0; i < MAX_FILES; i++) clear_slot(i);
    file_count = 1;
    current_dir = 0;
    str_copy16(files[0].name, "/");
    files[0].kind = FS_KIND_DIR;
    files[0].alive = 1;
    files[0].parent = 0;
}

static int alloc_slot(void) {
    for (int i = 0; i < file_count; i++) {
        if (!files[i].alive) return i;
    }
    if (file_count >= MAX_FILES) return -1;
    return file_count++;
}

static int find_child(int parent, char* name, uint8_t kind) {
    for (int i = 0; i < file_count; i++) {
        if (!files[i].alive) continue;
        if (files[i].parent != parent) continue;
        if (!str_eq(files[i].name, name)) continue;
        if (kind != 0xFF && files[i].kind != kind) continue;
        return i;
    }
    return -1;
}

static int parent_of(int idx) {
    if (idx <= 0 || idx >= file_count) return 0;
    if (!files[idx].alive) return 0;
    return files[idx].parent;
}

static int next_segment(const char* path, int* pos, char* seg, int* more) {
    int i = *pos;
    while (path[i] == '/') i++;
    if (!path[i]) {
        seg[0] = 0;
        *more = 0;
        *pos = i;
        return 0;
    }

    int j = 0;
    while (path[i] && path[i] != '/') {
        if (j < 15) seg[j++] = path[i];
        i++;
    }
    seg[j] = 0;

    int k = i;
    while (path[k] == '/') k++;
    *more = path[k] != 0;
    *pos = k;
    return 1;
}

static int create_dir_entry(int parent, char* name) {
    int idx = alloc_slot();
    if (idx < 0) return -1;
    clear_slot(idx);
    str_copy16(files[idx].name, name);
    files[idx].kind = FS_KIND_DIR;
    files[idx].alive = 1;
    files[idx].parent = parent;
    return idx;
}

int resolve_dir_path(char* path, int create_missing) {
    fs_init_root();

    if (!path || !path[0]) return current_dir;

    int cur = (path[0] == '/') ? 0 : current_dir;
    int pos = 0;

    while (1) {
        char seg[16];
        int more = 0;

        if (!next_segment(path, &pos, seg, &more)) break;

        if (str_eq(seg, ".")) {
            if (!more) break;
            continue;
        }

        if (str_eq(seg, "..")) {
            cur = parent_of(cur);
            if (!more) break;
            continue;
        }

        int child = find_child(cur, seg, 0xFF);

        if (child < 0) {
            if (!create_missing) return -1;
            child = create_dir_entry(cur, seg);
            if (child < 0) return -1;
        } else if (files[child].kind != FS_KIND_DIR) {
            return -1;
        }

        cur = child;

        if (!more) break;
    }

    return cur;
}

static int resolve_parent_path(char* path, int create_missing, char* leaf) {
    fs_init_root();

    if (!path || !path[0]) {
        leaf[0] = 0;
        return -1;
    }

    int cur = (path[0] == '/') ? 0 : current_dir;
    int pos = 0;

    while (1) {
        char seg[16];
        int more = 0;

        if (!next_segment(path, &pos, seg, &more)) {
            leaf[0] = 0;
            return -1;
        }

        if (!more) {
            str_copy16(leaf, seg);
            return cur;
        }

        if (str_eq(seg, ".")) continue;

        if (str_eq(seg, "..")) {
            cur = parent_of(cur);
            continue;
        }

        int child = find_child(cur, seg, 0xFF);

        if (child < 0) {
            if (!create_missing) return -1;
            child = create_dir_entry(cur, seg);
            if (child < 0) return -1;
        } else if (files[child].kind != FS_KIND_DIR) {
            return -1;
        }

        cur = child;
    }
}

static void write_entry(uint8_t** ptr, File* f) {
    **ptr = f->alive;
    (*ptr)++;
    **ptr = f->kind;
    (*ptr)++;
    **ptr = f->parent;
    (*ptr)++;
    **ptr = 0;
    (*ptr)++;
    *(uint32_t*)(*ptr) = (uint32_t)f->size;
    *ptr += 4;
    for (int i = 0; i < 16; i++) {
        **ptr = f->name[i];
        (*ptr)++;
    }
    if (f->alive && f->kind != FS_KIND_DIR) {
        for (int i = 0; i < f->size; i++) {
            **ptr = f->data[i];
            (*ptr)++;
        }
    }
}

static int read_entry_count(uint8_t** ptr) {
    int count = *(uint32_t*)(*ptr);
    *ptr += 4;
    return count;
}

static void create_common(char* name, uint8_t* data, int size, uint8_t kind) {
    fs_init_root();

    char leaf[16];
    int parent = resolve_parent_path(name, 1, leaf);
    if (parent < 0 || !leaf[0]) return;

    int idx = find_child(parent, leaf, 0xFF);

    if (idx >= 0) {
        if (files[idx].kind == FS_KIND_DIR) return;
        if (files[idx].data) free(files[idx].data);
    } else {
        idx = alloc_slot();
        if (idx < 0) return;
        clear_slot(idx);
    }

    str_copy16(files[idx].name, leaf);
    files[idx].kind = kind;
    files[idx].alive = 1;
    files[idx].parent = parent;
    files[idx].size = size;
    files[idx].data = malloc(size);
    if (!files[idx].data) {
        files[idx].alive = 0;
        files[idx].size = 0;
        return;
    }

    for (int i = 0; i < size; i++)
        files[idx].data[i] = data[i];
}

void create(char* name, char* data) {
    int size = str_len(data) + 1;
    create_common(name, (uint8_t*)data, size, FS_KIND_FILE);
}

void create_bin(char* name, uint8_t* data, int size) {
    create_common(name, data, size, FS_KIND_BIN);
}

struct File* read_file(char* name) {
    fs_init_root();

    char leaf[16];
    int parent = resolve_parent_path(name, 0, leaf);
    if (parent < 0 || !leaf[0]) return 0;

    int idx = find_child(parent, leaf, FS_KIND_FILE);
    if (idx < 0) return 0;
    return &files[idx];
}

struct File* read_bin(char* name) {
    fs_init_root();

    char leaf[16];
    int parent = resolve_parent_path(name, 0, leaf);
    if (parent < 0 || !leaf[0]) return 0;

    int idx = find_child(parent, leaf, FS_KIND_BIN);
    if (idx < 0) return 0;
    return &files[idx];
}

void delete_file(char* name) {
    fs_init_root();

    char leaf[16];
    int parent = resolve_parent_path(name, 0, leaf);
    if (parent < 0 || !leaf[0]) return;

    int idx = find_child(parent, leaf, 0xFF);
    if (idx < 0) return;

    if (files[idx].kind == FS_KIND_DIR) {
        for (int i = 0; i < file_count; i++) {
            if (!files[i].alive) continue;
            if (files[i].parent == idx) return;
        }
    }

    if (files[idx].data) free(files[idx].data);
    clear_slot(idx);

    while (file_count > 1 && !files[file_count - 1].alive)
        file_count--;

    if (current_dir >= file_count || !files[current_dir].alive)
        current_dir = 0;
}

int mkdir_path(char* path) {
    int idx = resolve_dir_path(path, 1);
    return idx < 0 ? -1 : 0;
}

int cd_path(char* path) {
    int idx = resolve_dir_path(path, 0);
    if (idx < 0) return -1;
    current_dir = idx;
    return 0;
}

int fs_open(char* path, int mode) {
    char leaf[16];
    int parent = resolve_parent_path(path, 1, leaf);
    if (parent < 0 || !leaf[0]) return -1;
    int idx = find_child(parent, leaf, 0xFF);
    if (mode == 0) {
        if (idx < 0) return -1;
        return idx;
    }
    if (mode == 1) {
        if (idx >= 0) {
            if (files[idx].data) free(files[idx].data);
            files[idx].data = 0;
            files[idx].size = 0;
        } else {
            idx = alloc_slot();
            if (idx < 0) return -1;
            clear_slot(idx);
            str_copy16(files[idx].name, leaf);
            files[idx].kind = FS_KIND_FILE;
            files[idx].alive = 1;
            files[idx].parent = parent;
        }

        return idx;
    }

    return -1;
}

void load_external_bin(char* name, int sector) {
    uint8_t buffer[512];
    read_sector(sector, buffer);

    int size = *(int*)buffer;
    if (size <= 0) return;

    uint8_t* data = malloc(size);
    if (!data) return;

    int read = 0;
    int s = sector;

    while (read < size) {
        read_sector(s, buffer);
        int start = (s == sector) ? 4 : 0;

        for (int i = start; i < 512 && read < size; i++) {
            data[read++] = buffer[i];
        }

        s++;
    }

    create_bin(name, data, size);
}

void save_fs() {
    static uint8_t buffer[512 * FS_SECTORS];

    for (int i = 0; i < sizeof(buffer); i++)
        buffer[i] = 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + sizeof(buffer);

    if (ptr + 4 > end) return;
    *(uint32_t*)ptr = FS_MAGIC;
    ptr += 4;

    if (ptr + 4 > end) return;
    *(uint32_t*)ptr = file_count;
    ptr += 4;

    for (int i = 0; i < file_count; i++) {
        if (ptr + 24 > end) return;
        write_entry(&ptr, &files[i]);
    }

    for (int i = 0; i < FS_SECTORS; i++)
        write_sector(1 + i, buffer + i * 512);
}

void load_fs() {
    static uint8_t buffer[512 * FS_SECTORS];

    for (int i = 0; i < FS_SECTORS; i++)
        read_sector(1 + i, buffer + i * 512);

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + sizeof(buffer);

    if (ptr + 8 > end) {
        fs_reset();
        return;
    }

    uint32_t magic = *(uint32_t*)ptr;
    ptr += 4;

    if (magic != FS_MAGIC) {
        fs_reset();
        return;
    }

    int count = read_entry_count(&ptr);
    if (count < 1 || count > MAX_FILES) {
        fs_reset();
        return;
    }

    for (int i = 0; i < MAX_FILES; i++) clear_slot(i);

    file_count = count;
    current_dir = 0;

    for (int i = 0; i < count; i++) {
        if (ptr + 24 > end) break;

        files[i].alive = *ptr++;
        files[i].kind = *ptr++;
        files[i].parent = *ptr++;
        ptr++;
        files[i].size = *(uint32_t*)ptr;
        ptr += 4;

        for (int j = 0; j < 16; j++)
            files[i].name[j] = *ptr++;

        if (files[i].alive && files[i].kind != FS_KIND_DIR) {
            if (files[i].size <= 0 || ptr + files[i].size > end) {
                files[i].alive = 0;
                files[i].size = 0;
                files[i].data = 0;
                break;
            }

            files[i].data = malloc(files[i].size);
            if (!files[i].data) {
                files[i].alive = 0;
                files[i].size = 0;
                break;
            }

            for (int j = 0; j < files[i].size; j++)
                files[i].data[j] = *ptr++;
        } else {
            files[i].data = 0;
            files[i].size = 0;
        }
    }

    if (!files[0].alive || files[0].kind != FS_KIND_DIR) {
        fs_reset();
        return;
    }

    current_dir = 0;
}