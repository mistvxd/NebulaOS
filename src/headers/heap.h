#pragma once

#include <stdint.h>

void* malloc(uint32_t size);
void free(void* ptr);
void heap_init(void);