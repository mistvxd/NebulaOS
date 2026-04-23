#include "headers/tss.h"

struct TSS tss;

void tss_init(uint32_t kernel_stack_top) {
    uint8_t* ptr = (uint8_t*)&tss;
    for (uint32_t i = 0; i < sizeof(struct TSS); i++) ptr[i] = 0;

    tss.ss0 = 0x10;
    tss.esp0 = kernel_stack_top;
    tss.iomap_base = sizeof(struct TSS);
}

void tss_flush() {
    asm volatile("mov $0x2B, %%ax; ltr %%ax" ::: "ax");
}