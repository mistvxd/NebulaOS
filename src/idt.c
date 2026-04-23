#include "headers/idt.h"
#include "headers/ports.h"
#include "headers/syscall.h"

extern void syscall_stub();
extern void kernel_main(int);
extern void vga_print_color(const char*, uint8_t);
extern void vga_print_hex(uint32_t);
extern void program_exit();

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

struct interrupt_frame {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t err_code;
    uint32_t eip, cs, eflags;
};

extern void idt_load(uint32_t);

extern void vga_print(const char*);

void pagefault_handler(struct interrupt_frame *frame) {
    vga_print_color("[ PROGRAM HALTED | #PF | Page Fault (0xE) ]", 0x0C);

    uint32_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    vga_print_color("\n[ ADDR: ", 0x0E);
    vga_print_hex(cr2);
    vga_print_color(" ]", 0x0E);

    vga_print_color("\n[ EIP: ", 0x0E);
    vga_print_hex(frame->eip);
    vga_print_color(" ]", 0x0E);

    vga_print_color("\n[ ERR: ", 0x0E);
    vga_print_hex(frame->err_code);
    vga_print_color(" ]\n", 0x0E);

    if ((frame->cs % 3) == 0)
        vga_print_color("from user mode\n", 0x0D);
    else
        vga_print_color("from kernel mode\n", 0x0D);

    process_exit();
}

void gpf_handler(struct interrupt_frame *frame) {
    vga_print_color("[ PROGRAM HALTED | #GP | General Protection Fault (0xD) ]", 0x0C);

    vga_print_color("\n[ EIP: ", 0x0E);
    vga_print_hex(frame->eip);
    vga_print_color(" ]", 0x0E);

    vga_print_color("\n[ CS: ", 0x0E);
    vga_print_hex(frame->cs);
    vga_print_color(" ]", 0x0E);

    vga_print_color("\n[ ERR: ", 0x0E);
    vga_print_hex(frame->err_code);
    vga_print_color(" ]", 0x0E);

    vga_print("\n");

    if ((frame->cs % 3) == 0)
        vga_print_color("from user mode\n", 0x0D);
    else
        vga_print_color("from kernel mode\n", 0x0D);

    process_exit();
}

void exception_handler(struct interrupt_frame *frame) {
    vga_print_color("\n[ PROGRAM HALTED | An exception occured.]", 0x0C);

    process_exit();
}

__attribute__((naked))
void irq_stub() {
    __asm__ volatile (
        "pusha\n"
        "call irq_handler\n"
        "popa\n"
        "iret\n"
    );
}

void irq_handler() {
    vga_print("PENIS");
    outb(0x20, 0x20);
}

__attribute__((naked))
void keyboard_stub() {
    __asm__ volatile (
        "pusha\n"
        "call keyboard_handler\n"
        "popa\n"
        "iret\n"
    );
}

void keyboard_handler() {
    //inb(0x60);
    outb(0x20, 0x20);
}

__attribute__((naked))
void isr_stub() {
    __asm__ volatile (
        "pusha\n"
        "push $0\n"
        "push $8\n"
        "push %esp\n"
        "call exception_handler\n"
        "add $12, %esp\n"
        "popa\n"
        "iret\n"
    );
}

__attribute__((naked))
void gpf_stub() {
    __asm__ volatile (
        "pusha\n"
        "mov %esp, %eax\n"
        "push %eax\n"
        "call gpf_handler\n"
        "add $4, %esp\n"

        "popa\n"
        "add $4, %esp\n"
        "iret\n"
    );
}

__attribute__((naked))
void pagefault_stub() {
    __asm__ volatile (
        "pusha\n"
        "mov %esp, %eax\n"
        "push %eax\n"
        "call pagefault_handler\n"
        "add $4, %esp\n"

        "popa\n"
        "add $4, %esp\n"
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
    for (int i = 0; i < 32; i++)
        idt_set(i, (uint32_t)isr_stub);
    for (int i = 32; i < 48; i++)
        idt_set(i, (uint32_t)irq_stub);
    idt_set(0x0D, (uint32_t)gpf_stub);
    idt_set(0x0E, (uint32_t)pagefault_stub);
    idt_set(0x80, (uint32_t)syscall_stub);
    idt_set(0x21, (uint32_t)keyboard_stub);
    idt[0x80].type_attr = 0xEE;
    idt_load((uint32_t)&idt_ptr);
}