#include <stdint.h>

extern void vga_print(const char*);

typedef struct regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
} regs_t;

void syscall_handler(regs_t* r) {
    uint32_t eax = r->eax;
    uint32_t ebx = r->ebx;
    uint32_t ecx = r->ecx;

    vga_print("\n");

    if (eax == 1) vga_print("EAX OK\n");
    else vga_print("EAX BAD\n");

    if (ebx == 5) vga_print("EBX OK\n");
    else vga_print("EBX BAD\n");

    if (ecx == 10) vga_print("ECX OK\n");
    else vga_print("ECX BAD\n");

    while (1);
}