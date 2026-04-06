#include <stdint.h>
#include "headers/filesystem.h"
#include "headers/disk.h"
#include "headers/heap.h"
#include "headers/gdt.h"
#include "headers/idt.h"
#include "headers/tss.h"
#include "headers/ports.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define MAX_COMMANDS 16
#define INPUT_MAX 128
#define USER_CODE 0x200000
void* user_target = 0;

volatile uint16_t* VGA_MEMORY = (uint16_t*)0xB8000;

int cursor_x = 0;
int cursor_y = 0;

int is_bin(struct File* f) {
    return ((uint8_t)f->name[0] == 0xFF);
}

char* fs_name(struct File* f) {
    if (is_bin(f)) return &f->name[1];
    return f->name;
}

uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ', 0x07);

    cursor_x = 0;
    cursor_y = 0;
}

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static void vga_scroll() {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] =
                VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', 0x07);
    }

    if (cursor_y > 0)
        cursor_y--;
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

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    }
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', 0x0F);
        }
    }
    else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, 0x0F);
        cursor_x++;

        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    if (cursor_y >= VGA_HEIGHT)
        vga_scroll();
}

void vga_putchar_color(char c, uint8_t color) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    }
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', color);
        }
    }
    else {
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
        vga_putchar(str[i]);
}

void vga_print_color(const char* str, uint8_t color) {
    for (int i = 0; str[i]; i++)
        vga_putchar_color(str[i], color);
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

char input_buffer[INPUT_MAX];
int input_pos = 0;

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

void memcpy(void* dest, void* src, uint32_t size) {
    uint8_t* d = dest;
    uint8_t* s = src;

    for (uint32_t i = 0; i < size; i++)
        d[i] = s[i];
}

void input_finalize() {
    input_buffer[input_pos] = 0;
}

void itoa(int num, char* str) {
    int i = 0;
    int negative = 0;

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

void wait_enter() {
    vga_print("[ENTER]\n");

    while (1) {
        char c = keyboard_getchar();
        if (c == '\n')
            break;
    }
}

typedef void (*command_func)(char*);

struct Command {
    char name[16];
    command_func func;
};

struct Command commands[MAX_COMMANDS];
int command_count = 0;

void register_command(char* name, command_func func) {
    if (command_count >= MAX_COMMANDS) return;

    int i = 0;
    while (name[i]) {
        commands[command_count].name[i] = name[i];
        i++;
    }
    commands[command_count].name[i] = 0;

    commands[command_count].func = func;
    command_count++;
}

int str_eq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

void cmd_clear(char* args) {
    vga_clear();
}

void cmd_cat(char* args) {
    struct File* f = read_file(args);

    if (!f)
        f = read_bin(args);

    if (!f) {
        vga_print("not found\n");
        return;
    }

    if (is_bin(f)) {
        vga_print("[binary file]\n");
        return;
    }

    vga_print(f->data);
    vga_print("\n");
}

void cmd_ls(char* args) {
    for (int i = 0; i < file_count; i++) {
        struct File* f = &files[i];

        vga_print("-> ");

        if (is_bin(f))
            vga_print("[BIN] ");
        else
            vga_print("[STR] ");

        vga_print(fs_name(f));
        vga_print("\n");
    }
}

void cmd_hexdump(char* args) {
    struct File* f = read_bin(args);

    if (!f) {
        vga_print("not found or not binary\n");
        return;
    }

    char buf[16];

    for (int i = 0; i < f->size; i++) {
        itoa(f->data[i], buf);
        vga_print(buf);
        vga_print(" ");

        if ((i % 16) == 15)
            vga_print("\n");
    }

    vga_print("\n");
}

void cmd_edit(char* args) {
    vga_clear();
    vga_print("editing: ");
    vga_print(args);
    vga_print("\n\n");

    char buffer[256];
    int pos = 0;

    struct File* f = read_file(args);

    if (f) {
        for (int i = 0; f->data[i] && i < 255; i++) {
            buffer[pos++] = f->data[i];
            vga_putchar(f->data[i]);
            vga_update_cursor();
        }
    }

    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\n') {
            buffer[pos] = 0;

            create(args, buffer);
            save_fs();

            vga_print("saved\n");
            vga_clear();
            return;
        }
        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                vga_putchar('\b');
                vga_update_cursor();
            }
        }
        else {
            if (pos < 255) {
                buffer[pos++] = c;
                vga_putchar(c);
                vga_update_cursor();
            }
        }
    }
}

int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void cmd_editbin(char* args) {
    vga_clear();
    vga_print("editbin: ");
    vga_print(args);
    vga_print("\n\n");

    uint8_t buffer[256];
    int size = 0;

    struct File* f = read_bin(args);

    if (f) {
        size = f->size;

        for (int i = 0; i < size && i < 256; i++)
            buffer[i] = f->data[i];

        vga_print("existing:\n");

        char num[16];
        for (int i = 0; i < size; i++) {
            itoa(buffer[i], num);
            vga_print(num);
            vga_print(" ");

            if ((i % 16) == 15)
                vga_print("\n");
        }

        vga_print("\n\n");
    }

    vga_print("enter hex: \n");
    vga_update_cursor();

    char input[INPUT_MAX];
    int pos = 0;
    while (1) {
        char c = keyboard_getchar();
        if (!c) continue;

        if (c == '\n') {
            input[pos] = 0;

            size = 0;

            for (int i = 0; input[i] && size < 256;) {

                while (input[i] == ' ') i++;

                if (!input[i]) break;

                char c1 = input[i++];
                char c2 = input[i++];

                int hi = hex_to_int(c1);
                int lo = hex_to_int(c2);

                buffer[size++] = (hi << 4) | lo;
            }

            create_bin(args, buffer, size);
            save_fs();

            vga_print("\nsaved\n");
            vga_clear();
            return;
        }

        else if (c == '\b') {
            if (pos > 0) {
                pos--;
                vga_putchar('\b');
                vga_update_cursor();
            }
        }

        else {
            if (pos < INPUT_MAX - 1) {
                input[pos++] = c;
                vga_putchar(c);
                vga_update_cursor();
            }
        }
    }
}

void cmd_echo(char* args) {
    vga_print(args);
    vga_print("\n");
}

void cmd_delete(char* args) {
    delete_file(args);
    save_fs();
}

__attribute__((noreturn))
void enter_user_mode(void* entry) {
    static uint8_t user_stack[4096] __attribute__((aligned(16)));
    uint32_t user_sp = (uint32_t)(user_stack + sizeof(user_stack));

    vga_print("swapped to ring3.");

    asm volatile (
        "cli\n"
        "pushl $0x23\n"
        "pushl %1\n"
        "pushl $0x2\n"
        "pushl $0x1B\n"
        "pushl %0\n"
        "iret\n"
        :
        : "r"(entry), "r"(user_sp)
        : "memory"
    );

    __builtin_unreachable();
}

extern void user_stub();

void run_user(void* data, uint32_t size) {
    char buf[16];

    vga_print("swapping to ring3...\n");

    memcpy((void*)USER_CODE, data, size);

    vga_print("copied to: ");
    itoa(USER_CODE, buf);
    vga_print(buf);
    vga_print("\n");

    enter_user_mode((void*)USER_CODE);
}

void run_raw(struct File* f) {
    if (!f || f->size == 0) {
        vga_print("invalid\n");
        return;
    }

    run_user(f->data, f->size);

    vga_print("returned??\n");
}

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

void cmd_raw(char* args) {
    struct File* f = read_bin(args);

    if (!f) {
        vga_print("not found or not binary\n");
        return;
    }

    run_raw(f);
}

void test_user() {
    asm volatile (
        "int $0x80\n"
    );
}

void execute_command() {
    input_finalize();

    char* cmd = input_buffer;
    char* args = "";

    for (int i = 0; input_buffer[i]; i++) {
        if (input_buffer[i] == ' ') {
            input_buffer[i] = 0;
            args = &input_buffer[i + 1];
            break;
        }
    }

    for (int i = 0; i < command_count; i++) {
        if (str_eq(cmd, commands[i].name)) {
            commands[i].func(args);
            vga_update_cursor();
            return;
        }
    }

    vga_print("unknown\n");
}

void kernel_main(int x) {
    gdt_init();
    tss_init();
    tss_flush();
    idt_init();
    pic_remap();
    if (x == 1) {
        vga_clear();
    } else {
        vga_print("\n");
    }
    heap_init();
    load_fs();

    if (!read_file("test.vm")) {
        char prog[] =
            "p ola mundo\n"
            "n 123\n"
            "p funcionando\n"
            "halt\n";

        create("test.vm", prog);
        save_fs();
    }

    uint8_t buffer[512];
    read_sector(10, buffer);

    create_bin("prog.bin", buffer, 512);

    register_command("clear", cmd_clear);
    register_command("ls", cmd_ls);
    register_command("cat", cmd_cat);
    register_command("edit", cmd_edit);
    register_command("echo", cmd_echo);
    register_command("del", cmd_delete);
    register_command("hexdump", cmd_hexdump);
    register_command("editbin", cmd_editbin);
    register_command("raw", cmd_raw);

    if (x == 1) {
        vga_print_color("Kiwi System\n\n", 0x0A);
    }

    while (1) {
        vga_print_color("* > ", 0x02);
        vga_enable_cursor();
        vga_update_cursor();
        input_reset();

        while (1) {
            char c = keyboard_getchar();
            if (!c) continue;

            if (c == '\n') {
                vga_print("\n");
                vga_update_cursor();
                execute_command();
                break;
            }
            else if (c == '\b') {
                input_backspace();
                vga_putchar('\b');
                vga_update_cursor();
            }
            else {
                input_add(c);
                vga_putchar(c);
                vga_update_cursor();
            }
        }
    }
}