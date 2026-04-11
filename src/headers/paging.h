#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
extern uint32_t page_directory[1024];

void paging_init();
void map_kernel_page(uint32_t virt, uint32_t phys);
void map_user_page(uint32_t virt, uint32_t phys);

#endif