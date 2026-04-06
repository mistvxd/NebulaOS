#include <stdint.h>

extern char heap_start;
extern char heap_end;
extern void itoa(int num, char* str);
extern void debug_print(const char*);
extern void debug_print_num(int n);

typedef struct Block {
    uint32_t size;
    int free;
    struct Block* next;
} Block;

uint32_t align4(uint32_t size) {
    return (size + 3) & ~3;
}

static Block* heap_head = 0;

void heap_init() {
    debug_print("Initializing heap...\n");
    heap_head = (Block*)&heap_start;
    heap_head->size = ((uint32_t)&heap_end - (uint32_t)&heap_start) - sizeof(Block);
    heap_head->free = 1;
    heap_head->next = 0;
    debug_print("Heap initialized\n");
    debug_print("heap_start: ");
    char buf[16];
    itoa((int)&heap_start, buf);
    debug_print(buf);

    debug_print("\nheap_end: ");
    itoa((int)&heap_end, buf);
    debug_print(buf);
    debug_print("\n");
}

void split(Block* block, uint32_t size) {
    Block* new_block = (Block*)((char*)block + sizeof(Block) + size);

    new_block->size = block->size - size - sizeof(Block);
    new_block->free = 1;
    new_block->next = block->next;

    block->size = size;
    block->next = new_block;
}

void* malloc(uint32_t size) {
    size = align4(size);
    Block* curr = heap_head;
    while (curr) {  
        if (curr->free && curr->size >= size) {
            if (curr->size > size + sizeof(Block))
                split(curr, size);
            curr->free = 0;
            char buf3[16];
            itoa((int)curr + sizeof(Block), buf3);
            itoa((int)size, buf3);
            return (char*)curr + sizeof(Block);
        }
        curr = curr->next;
    }
    debug_print("malloc: failed\n");
    return 0;
}

void free(void* ptr) {
    if (!ptr) return;
    Block* block = (Block*)((char*)ptr - sizeof(Block));
    block->free = 1;
    Block* curr = heap_head;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += sizeof(Block) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}