#include <stdint.h>
#include "headers/filesystem.h"
#include "headers/heap.h"

extern void vga_putchar(char, uint8_t);
extern void process_exit();
extern void vga_print(const char*);
extern void vga_print_color(const char*, uint8_t);
extern void vga_clear();
extern void itoa(int, char*);
extern void vga_print_hex(uint32_t);
extern char keyboard_getchar();
extern char* current_user;
extern int is_root;
extern char user_path_base;
extern int cursor_x;
extern int cursor_y;
extern void strcat(char*, char*);
extern void memcpy(void*, void*, uint32_t);
extern int strcmp(char*, char*);

#define SYS_EXIT              1
#define SYS_WRITE             2
#define SYS_FS_READDIR        3
#define SYS_FS_CHDIR          4
#define SYS_FS_OPEN           5
#define SYS_FS_READ           6
#define SYS_FS_WRITE          7
#define SYS_READ              8
#define SYS_LOGIN             9
#define SYS_GET_CURRENT_USER  10
#define SYS_TERMINAL_CLEAR    11
#define SYS_MOVE_CURSOR       12

#define USER_MAX 0x400000

struct interrupt_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, useresp, ss;
};

struct dirent {
    char name[32];
    char owner[16];
    uint32_t kind;
};

static int valid_user_ptr(const void* ptr, uint32_t size) {
    uint32_t p = (uint32_t)ptr;
    if (p < 0x1000) return 0;
    if (p + size < p) return 0;
    if (p + size > USER_MAX) return 0;
    return 1;
}

static int copy_to_user(void* dst, const void* src, uint32_t n) {
    if (!valid_user_ptr(dst, n)) return -1;
    uint8_t* d = dst;
    const uint8_t* s = src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    return 0;
}

void copy_from_user(void* dst, void* src, uint32_t size) {
    uint8_t* d = dst;
    uint8_t* s = src;

    for (uint32_t i = 0; i < size; i++)
        d[i] = s[i];
}

static int copy_str_from_user(char* dst, const char* src, uint32_t max) {
    if (!valid_user_ptr(src, 1)) return -1;
    uint32_t i = 0;
    for (; i < max - 1; i++) {
        char c = src[i];
        dst[i] = c;
        if (!c) return 0;
    }
    dst[max - 1] = 0;
    return -1;
}

int sys_getchar() {
    char c;

    while (1) {
        c = keyboard_getchar();

        if (c) return c;
    }
}

uint32_t syscall_handler(struct interrupt_frame* frame) {
    switch (frame->eax) {

        case SYS_EXIT:
            process_exit();
            return 1;

        case SYS_WRITE: {
            char* str = (char*)frame->ebx;
            uint8_t color = frame->ecx & 0x0F;
            uint32_t len = frame->edx;

            if (!valid_user_ptr(str, len)) return -1;

            for (uint32_t i = 0; i < len; i++) {
                vga_putchar(str[i], color);
            }

            return 0;
        }

        case SYS_FS_READDIR: {
            char* path = (char*)frame->ebx;
            int index = frame->ecx;
            struct dirent* user_ent = (struct dirent*)frame->edx;
            int mode = frame->esi;

            int dir = current_dir;

            if (path && path[0]) {
                dir = resolve_dir_path(path, 0, 0);
                if (dir < 0) return 0;
            }

            int found = 0;

            for (int i = 0; i < file_count; i++) {
                if (!files[i].alive) continue;
                if (files[i].parent != dir) continue;
                if (i == dir) continue;

                if (found == index) {
                    struct dirent kent;

                    int j = 0;
                    while (files[i].name[j] && j < 15) {
                        kent.name[j] = files[i].name[j];
                        j++;
                    }
                    if (files[i].kind == FS_KIND_DIR) {
                        kent.name[j++] = '/';
                    }
                    int k = 0;
                    while (files[i].owner[k] && k < 15) {
                        kent.owner[k] = files[i].owner[k];
                        k++;
                    }
                    kent.name[j] = 0;
                    kent.owner[k] = 0;
                    kent.kind = files[i].kind;

                    copy_to_user(user_ent, &kent, sizeof(kent));
                    return files[i].size;
                }

                found++;
            }

            struct dirent empty;
            empty.name[0] = 0;
            empty.kind = 0;

            copy_to_user(user_ent, &empty, sizeof(empty));
            return 0;
        }

        case SYS_FS_CHDIR: {
            char path[256];

            if (copy_str_from_user(path, (char*)frame->ebx, sizeof(path)) < 0)
                return 0;

            cd_path(path);
            return 0;
        }

        case SYS_FS_OPEN: {
            char* user_path = (char*)frame->ebx;
            int mode = frame->ecx;
            char kpath[128];
            if (copy_str_from_user(kpath, user_path, sizeof(kpath)) < 0) {
                return -1;
            }
            return fs_open(kpath, mode);
        }

		case SYS_FS_READ: {
		    int idx = frame->ebx;
		    char* user_buf = (char*)frame->ecx;
		    int max = frame->edx;

            if (!valid_user_ptr(user_buf, max))
                return -1;

		    if (idx < 0 || idx >= file_count) return -1;
		    if (!files[idx].alive) return -1;
		    if (files[idx].kind == FS_KIND_DIR) return -1;

		    int size = files[idx].size;
		    if (size > max) size = max;

		    if (size > 0 && files[idx].data)
		        copy_to_user(user_buf, files[idx].data, size);

		    return size;
		}

		case SYS_FS_WRITE: {
		    int idx = frame->ebx;
		    char* user_buf = (char*)frame->ecx;
		    int size = frame->edx;
            
            if (!valid_user_ptr(user_buf, size))
                return -1;
            
		    if (idx < 0 || idx >= file_count) return -1;
		    if (!files[idx].alive) return -1;
		    if (files[idx].kind == FS_KIND_DIR) return -1;
		    if (size <= 0) return 0;

		    if (files[idx].data)
		        free(files[idx].data);

		    uint8_t* kbuf = malloc(size);
		    if (!kbuf) return -1;

		    copy_from_user(kbuf, user_buf, size);

		    files[idx].data = kbuf;
		    files[idx].size = size;

            save_fs();

		    return size;
		}

        case SYS_READ: {
            char* buffer = frame->ebx;
            char ret = sys_getchar();
            copy_to_user(buffer, &ret, 1);
            return 0;
        }

        case SYS_LOGIN: {
            char* username = (char*)frame->ebx;
            uint8_t kbuf[16];
            
            copy_from_user(kbuf, username, 16);
            is_root = false;
            if (strcmp(kbuf, "root") == 0) is_root = true;
            current_user = kbuf;
            if (!is_root) {
                char user_path[64];
                user_path[0] = '\0';
                strcat(user_path, &user_path_base);
                strcat(user_path, current_user);
                resolve_dir_path(user_path, 1, 1);
                cd_path(user_path);
                strcat(user_path, "/utils");
                resolve_dir_path(user_path, 1, 1);
            }
            else {
                cd_path("/");
            }
            return 0;
        }

        case SYS_GET_CURRENT_USER: {
            char* buffer = frame->ebx;
            copy_to_user(buffer, current_user, 16);
            return 0;
        }

        case SYS_TERMINAL_CLEAR: {
            vga_clear();
            return 0;
        }

        case SYS_MOVE_CURSOR: {
            int x = frame->ebx;
            int y = frame->ecx;

            if (x < 0) x = 0;
            if (x > 79) x = 79;
            if (y < 0) y = 0;
            if (y > 24) y = 24;

            cursor_x = x;
            cursor_y = y;
            return 0;
        }
    }

    return 0;
}