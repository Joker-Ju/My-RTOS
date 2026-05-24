#ifndef __HEAP_H
#define __HEAP_H

#ifndef HEAP_SIZE
    #define HEAP_SIZE 1024  // 堆大小
#endif

#include "sched.h"

typedef struct heap_block {
    uint32_t size;               // 本块大小（包含 header）
    struct heap_block *next;     // 空闲链表指针（只在空闲时有效）
} heap_block_t;


void heap_init(void);

void *heap_malloc(uint32_t size);

void heap_free(void *block);

#endif /* __HEAP_H */
