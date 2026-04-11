#include "headers/idt.h"
#include "headers/ports.h"
#include "headers/syscall.h"

extern void syscall_stub();
extern void kernel_main(int);
extern void vga_print_color(const char*, uint8_t);
extern void vga_print_hex(uint32_t);

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
    uint32_t int_no, err_code;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags;
};

const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check"
};

extern void idt_load(uint32_t);

extern void vga_print(const char*);

void exception_handler(struct interrupt_frame *frame) {
    vga_print_color("\n ---[ INTERRUPT FRAME ]--- \n", 0x0E);
    vga_print_color("EAX: ", 0x0E);
    vga_print_hex(frame->eax);
    vga_print_color(" EBX: ", 0x0E);
    vga_print_hex(frame->ebx);
    vga_print_color(" ECX: ", 0x0E);
    vga_print_hex(frame->ecx);
    vga_print_color(" EDX: ", 0x0E);
    vga_print_hex(frame->edx);
    vga_print("\n");
    vga_print_color("ESI: ", 0x0E);
    vga_print_hex(frame->esi);
    vga_print_color(" EDI: ", 0x0E);
    vga_print_hex(frame->edi);
    vga_print_color(" EBP: ", 0x0E);
    vga_print_hex(frame->ebp);
    vga_print_color(" ESP: ", 0x0E);
    vga_print_hex(frame->esp);
    vga_print("\n");
    vga_print_color("CS: ", 0x0E);
    vga_print_hex(frame->cs);
    vga_print_color(" EFLAGS: ", 0x0E);
    vga_print_hex(frame->eflags);
    vga_print("\n");
    vga_print_color("\n ---[ PROGRAM HALTED | Exception: ", 0x0C);
    vga_print_hex(frame->int_no);
    if (frame->int_no < 32) {
        vga_print(": ");
        vga_print_color(exception_messages[frame->int_no], 0x0E);
    }
    vga_print_color(" ]--- \n", 0x0C);
    vga_print_color("\n ---[ EIP: ", 0x0E);
    vga_print_hex(frame->eip);
    vga_print_color(" ]--- \n", 0x0E);

    while(1) {
        asm volatile("cli; hlt");
    }
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
        "push $0\n"
        "push $13\n"
        "push %esp\n"
        "call exception_handler\n"
        "add $12, %esp\n"
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