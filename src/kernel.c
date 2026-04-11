#include <stdint.h>
#include "headers/filesystem.h"
#include "headers/disk.h"
#include "headers/heap.h"
#include "headers/gdt.h"
#include "headers/idt.h"
#include "headers/tss.h"
#include "headers/ports.h"
#include "headers/paging.h"

typedef struct {
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
} jmp_buf;

jmp_buf kernel_ctx;

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define INPUT_MAX 128
#define USER_CODE 0x200000

uint32_t kernel_esp = 0;
uint32_t kernel_eip = 0;

char* path_dirs[] = {
    "/bin",
    "/",
    0
};

volatile int current_process_active = 0;

typedef struct {
    int active;
} process_t;

process_t process;
process_t* current_process = &process;

extern uint32_t stack_top;
uint32_t kernel_stack_top = 0x00109000;
uint32_t user_stack_top = 0x00210000;

volatile uint16_t* VGA_MEMORY = (uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

char input_buffer[INPUT_MAX];
int input_pos = 0;

uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

__attribute__((naked)) int setjmp(jmp_buf* buf) {
    asm volatile(
        "mov 4(%esp), %eax\n"
        "mov %esp, (%eax)\n"
        "mov %ebp, 4(%eax)\n"
        "mov (%esp), %ecx\n"
        "mov %ecx, 8(%eax)\n"
        "xor %eax, %eax\n"
        "ret\n"
    );
}

__attribute__((naked)) void longjmp(jmp_buf* buf) {
    asm volatile(
        "mov 4(%esp), %eax\n"
        "mov (%eax), %esp\n"
        "mov 4(%eax), %ebp\n"
        "mov 8(%eax), %ecx\n"
        "mov %ecx, (%esp)\n"
        "mov $1, %eax\n"
        "ret\n"
    );
}

void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ', 0x07);

    cursor_x = 0;
    cursor_y = 0;
}

static void vga_scroll() {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', 0x07);

    if (cursor_y > 0)
        cursor_y--;
}

void vga_putchar(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', color);
        }
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, color);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y >= VGA_HEIGHT)
        vga_scroll();
}

void vga_print(const char* str) {
    for (int i = 0; str[i]; i++)
        vga_putchar(str[i], 0x0F);
}

void vga_print_hex(uint32_t val) {
    char buf[9];
    for (int i = 0; i < 8; i++) {
        uint8_t byte = (val >> ((7 - i) * 4)) & 0xF;
        buf[i] = byte < 10 ? '0' + byte : 'A' + (byte - 10);
    }
    buf[8] = 0;
    vga_print(buf);
}

void vga_update_cursor() {
    uint16_t pos = cursor_y * 80 + cursor_x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);

    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
}

void vga_enable_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0);

    outb(0x3D4, 0x0B);
    outb(0x3D5, 15);
}

void vga_print_color(const char* str, uint8_t color) {
    for (int i = 0; str[i]; i++)
        vga_putchar(str[i], color);
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

char scancode_to_ascii(uint8_t sc) {
    static char map[128] = {
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=', '\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',
        0,'*',0,' '
    };

    if (sc > 57) return 0;
    return map[sc];
}

char keyboard_getchar() {
    while (!(inb(0x64) & 1));
    uint8_t sc = inb(0x60);
    if (sc & 0x80) return 0;
    return scancode_to_ascii(sc);
}

void input_reset() {
    input_pos = 0;
}

void input_add(char c) {
    if (input_pos < INPUT_MAX - 1)
        input_buffer[input_pos++] = c;
}

void input_backspace() {
    if (input_pos > 0)
        input_pos--;
}

void input_finalize() {
    input_buffer[input_pos] = 0;
}

void wait_for_enter() {
    while (keyboard_getchar() != '\n');
}

void memcpy(void* dest, void* src, uint32_t size) {
    uint8_t* d = dest;
    uint8_t* s = src;
    for (uint32_t i = 0; i < size; i++)
        d[i] = s[i];
}

void itoa(int num, char* str) {
    int i = 0, negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = 0;
        return;
    }

    if (num < 0) {
        negative = 1;
        num = -num;
    }

    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }

    if (negative)
        str[i++] = '-';

    str[i] = 0;

    for (int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = tmp;
    }
}

static int str_eq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

void process_exit() {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    longjmp(&kernel_ctx);
}

__attribute__((noreturn))
void enter_user_mode(void* entry, int argc, char** argv) {
    uint32_t user_sp = 0x00210000;
    uint32_t argv_ptrs[32];

    for (int i = argc - 1; i >= 0; i--) {
        int len = 0;
        while (argv[i][len]) len++;
        len++;

        user_sp -= len;
        memcpy((void*)user_sp, argv[i], len);
        argv_ptrs[i] = user_sp;
    }

    user_sp &= ~3;

    user_sp -= 4;
    *(uint32_t*)user_sp = 0;

    for (int i = argc - 1; i >= 0; i--) {
        user_sp -= 4;
        *(uint32_t*)user_sp = argv_ptrs[i];
    }

    uint32_t argv_ptr = user_sp;

    user_sp -= 4;
    *(uint32_t*)user_sp = argv_ptr;

    user_sp -= 4;
    *(uint32_t*)user_sp = argc;

    user_sp -= 4;
    *(uint32_t*)user_sp = 0;

    asm volatile (
        "cli\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl $0x23\n"
        "pushl %1\n"
        "pushl $0x2\n"
        "pushl $0x1B\n"
        "pushl %0\n"
        "iret\n"
        :
        : "r"(entry), "r"(user_sp)
        : "memory", "ax"
    );

    __builtin_unreachable();
}

void run_user(void* data, uint32_t size, int argc, char** argv) {
    memcpy((void*)USER_CODE, data, size);

    if (setjmp(&kernel_ctx) == 0) {
        current_process_active = 1;
        enter_user_mode((void*)USER_CODE, argc, argv);
    } else {
        current_process_active = 0;
        return;
    }
}

void run_raw(struct File* f, int argc, char** argv) {
    if (!f || f->size == 0) {
        vga_print("invalid\n");
        return;
    }
    run_user(f->data, f->size, argc, argv);
}

static struct File* find_program(char* name) {
    struct File* f;

    for (int i = 0; path_dirs[i]; i++) {
        char full[INPUT_MAX];
        int pos = 0;

        char* dir = path_dirs[i];
        int j = 0;

        while (dir[j]) full[pos++] = dir[j++];
        if (pos > 0 && full[pos-1] != '/') full[pos++] = '/';

        j = 0;
        while (name[j]) full[pos++] = name[j++];

        full[pos] = 0;

        f = read_bin(full);
        if (f) return f;

        int k = pos;
        full[k++] = '.';
        full[k++] = 'b';
        full[k++] = 'i';
        full[k++] = 'n';
        full[k] = 0;

        f = read_bin(full);
        if (f) return f;
    }

    return 0;
}

void print_path(uint8_t color) {
    int stack[32];
    int sp = 0;

    int cur = current_dir;

    while (cur != 0 && sp < 32) {
        stack[sp++] = cur;
        cur = files[cur].parent;
    }

    vga_print_color("/", color);

    for (int i = sp - 1; i >= 0; i--) {
        vga_print_color(files[stack[i]].name, color);
        if (i > 0) vga_print_color("/", color);
    }
}

void shell() {
    while (1) {
        print_path(0x02);
        vga_print_color(" > ", 0x0A);
        vga_enable_cursor();
        vga_update_cursor();
        input_reset();

        while (1) {
            char c = keyboard_getchar();
            if (!c) continue;

            if (c == '\n') {
                vga_print("\n");
                input_finalize();

                char* argv[32];
                int argc = 0;
                char* ptr = input_buffer;

                while (*ptr) {
                    while (*ptr == ' ') {
                        *ptr = '\0';
                        ptr++;
                    }
                    if (*ptr == '\0') break;
                    if (argc < 32) {
                        argv[argc++] = ptr;
                    }
                    while (*ptr && *ptr != ' ') {
                        ptr++;
                    }
                }

                if (argc > 0) {
                    struct File* prog = find_program(argv[0]);
                    if (prog) {
                        run_raw(prog, argc, argv);
                        asm volatile("sti");

                        while (current_process_active) {
                            asm volatile("hlt");
                        }
                    } else {
                        vga_print("not found\n");
                    }
                }
                break;
            } else if (c == '\b') {
                input_backspace();
                vga_putchar('\b', 0x0F);
                vga_update_cursor();
            } else {
                input_add(c);
                vga_putchar(c, 0x0F);
                vga_update_cursor();
            }
        }
    }
}

void kernel_main(int x) {
    gdt_init();
    tss_init((uint32_t)&stack_top);
    tss_flush();
    idt_init();
    paging_init();
    for (uint32_t i = 0; i < 16 * 1024 * 1024; i += 0x1000) {
        map_kernel_page(i, i);
    }
    for (uint32_t i = 0x00200000; i < 0x00300000; i += 0x1000) {
        map_user_page(i, i);
    }
    for (uint32_t i = 0x00108000; i < 0x0010A000; i += 0x1000) {
        map_user_page(i, i);
    }
    for (uint32_t i = 0x00100000; i < 0x00110000; i += 0x1000) {
        map_user_page(i, i);
    }
    if (x == 1) vga_clear();
    else vga_print("\n");

    heap_init();
    load_fs();

    resolve_dir_path("/teste", 1);

    uint8_t buffer[512];
    read_sector(10, buffer);

    create_bin("prog.bin", buffer, 512);
    save_fs();

    read_sector(11, buffer);

    create_bin("welcome.bin", buffer, 512);
    save_fs();

    read_sector(12, buffer);

    create_bin("cd.bin", buffer, 512);
    save_fs();

    read_sector(13, buffer);

    create_bin("ls.bin", buffer, 512);
    save_fs();

    if (x == 1) {
        run_raw(find_program("welcome.bin"), 1, (char*[1]){"test"});
    }

    shell();
}