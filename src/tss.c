#include "headers/tss.h"

struct TSS tss;

static uint8_t kernel_stack[4096];

void tss_flush() {
    asm volatile (
        "mov $0x28, %%ax\n"
        "ltr %%ax\n"
        :
        :
        : "ax"
    );
}

void tss_init() {
    for (int i = 0; i < sizeof(struct TSS); i++)
        ((uint8_t*)&tss)[i] = 0;
    tss.ss0 = 0x10;
    static uint8_t kernel_stack[4096];
    tss.esp0 = (uint32_t)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(struct TSS);
}
