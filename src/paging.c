#include "headers/paging.h"
#include <stdint.h>

#define PAGE_PRESENT 1
#define PAGE_RW 2
#define PAGE_USER 4

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));

static uint32_t* get_page_table(uint32_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t* table;

    if (page_directory[pd_index] & PAGE_PRESENT) {
        table = (uint32_t*)(page_directory[pd_index] & 0xFFFFF000);
    } else {
        static uint32_t new_tables[16][1024] __attribute__((aligned(4096)));
        static int used = 0;

        table = new_tables[used++];
        for (int i = 0; i < 1024; i++) table[i] = 0;

        page_directory[pd_index] = ((uint32_t)table) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    return table;
}

void map_kernel_page(uint32_t virt, uint32_t phys) {
    uint32_t* table = get_page_table(virt);

    uint32_t pt_index = (virt >> 12) & 0x03FF;
    table[pt_index] = (phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW;
}

void map_user_page(uint32_t virt, uint32_t phys) {
    uint32_t* table = get_page_table(virt);

    uint32_t pt_index = (virt >> 12) & 0x03FF;
    table[pt_index] = (phys & 0xFFFFF000) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
}

void paging_init() {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
        first_page_table[i] = 0;
    }

    for (uint32_t i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_RW;
    }

    page_directory[0] = ((uint32_t)first_page_table) | PAGE_PRESENT | PAGE_RW | PAGE_USER;

    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}