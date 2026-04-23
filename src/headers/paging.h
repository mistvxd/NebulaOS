#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 1
#define PAGE_RW 2
#define PAGE_USER 4

#define PAGE_SIZE 4096

void paging_init();

uint32_t* get_kernel_pd();

uint32_t* create_page_directory();

void map_page(uint32_t* pd, uint32_t virt, uint32_t phys, int user);

uint32_t alloc_page();

uint32_t read_cr3();
void load_cr3(uint32_t* pd);

#endif