#include "heap.h"

static uint8_t heap_mem[HEAP_SIZE];   // 模块内部私有，外部看不见
static heap_block_t *head = NULL;

#define MIN_BLOCK_SIZE  (sizeof(heap_block_t) * 2)  // 最小块大小：header + 最少 1 字节数据（为了避免过度碎片化）

void heap_init(void)                  // 不需要参数了
{
    heap_block_t *first = (heap_block_t *)heap_mem;
    first->size = HEAP_SIZE;
    first->next = NULL;
    head = first;
}

void *heap_malloc(uint32_t size)
{
    if (size == 0)
        return NULL;

    uint32_t aligned = (size + 3) & ~3U;  // 4 字节对齐
    uint32_t needed  = sizeof(heap_block_t) + aligned;

    uint32_t bp = enter_critical();

    heap_block_t **prev = &head;
    heap_block_t  *curr = head;

    while (curr != NULL) {
        if (curr->size >= needed) {
            uint32_t remaining = curr->size - needed;

            if (remaining >= MIN_BLOCK_SIZE) {
                // 分割：前半分配，后半变新空闲块
                /* 注意：new_free 的地址 = curr + needed（单位是字节）
                所以要强转为 uint8_t* 来算字节偏移，再转回 heap_block_t*当作结构体指针*/
                heap_block_t *new_free = (heap_block_t *)((uint8_t *)curr + needed);
                new_free->size = remaining;
                new_free->next = curr->next;

                curr->size = needed;
                *prev = new_free;
            } 
            else {
                // 剩余不够一个块，整块分配
                *prev = curr->next;
            }

            exit_critical(bp);
            return (uint8_t *)curr + sizeof(heap_block_t);
        }

        prev = &curr->next;
        curr = curr->next;
    }

    exit_critical(bp);
    return NULL;
}

void heap_free(void *ptr)
{
    if (ptr == NULL)
        return;
    // 计算出块头地址
    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - sizeof(heap_block_t));

    uint32_t bp = enter_critical();

    // 按地址排序找插入位置
    heap_block_t **p = &head;
    while (*p != NULL && *p < block)
        p = &(*p)->next;

    // 尝试与后面合并
    if ((uint8_t *)block + block->size == (uint8_t *)*p) {
        block->size += (*p)->size;
        block->next  = (*p)->next;
    } 
    else {
        block->next = *p;
    }
    // p的值是指向前一个块的 next 指针的地址，所以直接 *p = block 就把 block 插入链表了
    *p = block;   // 插入空闲链表

    // 尝试与前面合并
    if (head != block) {
        heap_block_t *prev = head;
        while (prev != NULL && prev->next != block)
            prev = prev->next;

        if (prev != NULL) {
            uint32_t prev_size = prev->size;
            if ((uint8_t *)prev + prev_size == (uint8_t *)block) {
                prev->size += block->size;
                prev->next  = block->next;
            }
        }
    }

    exit_critical(bp);
}
