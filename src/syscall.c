#include <stdint.h>

extern void vga_print(const char*);
extern void vga_print_color(const char*, uint8_t);
extern void vga_putchar(char, uint8_t);
extern void itoa(int, char*);
extern void kernel_main(int x);

void debug(uint32_t val) {
    char buf[32];
    itoa(val, buf);
    vga_print(buf);
    vga_print("\n");
}

struct interrupt_frame {
    uint32_t gs, fs, es, ds;

    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;

    uint32_t eip, cs, eflags, useresp, ss;
};

void syscall_handler(struct interrupt_frame* frame) {
    switch (frame->eax) {
        case 1:
            kernel_main(0);
            break;
        case 2: {
            char* str = (char*)frame->ebx;
            uint8_t color = frame->ecx & 0x0F;
            uint32_t len = frame->edx;
            for (uint32_t i = 0; i < len; i++) {
                vga_putchar(str[i], color);
            }
            vga_print("\n");
            break;
        }
        default:
            break;
    }
}