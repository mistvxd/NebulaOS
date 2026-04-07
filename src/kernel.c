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
#define INPUT_MAX 128
#define USER_CODE 0x200000

extern uint32_t stack_top;

volatile uint16_t* VGA_MEMORY = (uint16_t*)0xB8000;
int cursor_x = 0;
int cursor_y = 0;

char input_buffer[INPUT_MAX];
int input_pos = 0;

uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
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

void run_user(void* data, uint32_t size) {
    memcpy((void*)USER_CODE, data, size);
    extern void enter_user_mode(void* entry);
    enter_user_mode((void*)USER_CODE);
}

void run_raw(struct File* f) {
    if (!f || f->size == 0) {
        vga_print("invalid\n");
        return;
    }

    run_user(f->data, f->size);
}

static struct File* find_program(char* name) {
    struct File* f = read_bin(name);
    if (f) return f;

    char alt[INPUT_MAX];
    int i = 0;

    while (name[i] && i < INPUT_MAX - 5) {
        alt[i] = name[i];
        i++;
    }

    alt[i++] = '.';
    alt[i++] = 'b';
    alt[i++] = 'i';
    alt[i++] = 'n';
    alt[i] = 0;

    return read_bin(alt);
}

__attribute__((noreturn))
void enter_user_mode(void* entry) {
    static uint8_t user_stack[4096] __attribute__((aligned(16)));
    uint32_t user_sp = (uint32_t)(user_stack + sizeof(user_stack));

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

void kernel_main(int x) {
    gdt_init();
    tss_init((uint32_t)&stack_top);
    tss_flush();
    idt_init();
    if (x == 1) vga_clear();
    else vga_print("\n");

    heap_init();
    load_fs();

    uint8_t buffer[512];
    read_sector(10, buffer);

    create_bin("prog.bin", buffer, 512);
    save_fs();

    read_sector(11, buffer);

    create_bin("welcome.bin", buffer, 512);
    save_fs();

    if (x == 1) {
        run_raw(find_program("welcome.bin"));
    }

    while (1) {
        vga_print_color("> ", 0x02);
        vga_enable_cursor();
        vga_update_cursor();
        input_reset();

        while (1) {
            char c = keyboard_getchar();
            if (!c) continue;

            if (c == '\n') {
                vga_print("\n");
                input_finalize();

                struct File* prog = find_program(input_buffer);
                if (!prog) {
                    vga_print("not found\n");
                    break;
                }

                run_raw(prog);
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