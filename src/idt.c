#include "headers/idt.h"
#include "headers/ports.h"
#include "headers/syscall.h"

extern void syscall_stub();
extern void kernel_main(int);

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct IDTEntry idt[256];
struct IDTPtr idt_ptr;

typedef struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} regs_t;

extern void idt_load(uint32_t);

extern void vga_print(const char*);

void crash() {
    vga_print("EXCEPTION\n");
    kernel_main(0);
}

__attribute__((naked))
void irq0_stub() {
    __asm__ volatile (
        "pusha\n"
        "call irq0_handler\n"
        "popa\n"
        "iret\n"
    );
}

void irq0_handler() {
    outb(0x20, 0x20);
}

__attribute__((naked))
void isr_stub() {
    __asm__ volatile (
        "pusha\n"
        "call crash\n"
        "popa\n"
        "iret\n"
    );
}

void idt_set(int i, uint32_t handler) {
    idt[i].offset_low = handler & 0xFFFF;
    idt[i].selector = 0x08;
    idt[i].zero = 0;
    idt[i].type_attr = 0x8E;
    idt[i].offset_high = (handler >> 16) & 0xFFFF;
}

void idt_init() {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    for (int i = 0; i < 256; i++)
        idt_set(i, (uint32_t)isr_stub);
    idt_set(0x20, (uint32_t)irq0_stub);
    idt_set(0x80, (uint32_t)syscall_stub);
    idt[0x80].type_attr = 0xEE;
    idt_load((uint32_t)&idt_ptr);
}