#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "myalloc.h"

#define BLOCK_MAGIC 0x4D414C4C
#define MIN_GROW    (64 * 1024)

typedef struct block {
    size_t        size;    /* payload size in bytes             */
    struct block *next;    /* next block, in increasing address */
    int           free;    /* 1 = free, 0 = in use              */
    int           magic;   /* constant to detect bad pointers   */
} block_t;

#define ALIGN_UP(n) (((size_t)(n) + 15) & ~(size_t)15)
#define HDR         ALIGN_UP(sizeof(block_t))

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t total_heap_bytes = 0;

static inline size_t align_up(size_t n)
{
    return (n + 15) & ~(size_t)15;
}

static inline void *payload(block_t *b)
{
    return (void *)((char *)b + HDR);
}

static inline block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

static block_t *grow(size_t need)
{
    void *cur_brk = sbrk(0);
    if (cur_brk == (void *)-1) {
        return NULL;
    }

    uintptr_t addr = (uintptr_t)cur_brk;
    size_t pad = (16 - (addr % 16)) % 16;

    size_t payload_size = (need > MIN_GROW) ? need : MIN_GROW;
    size_t total_req = pad + HDR + payload_size;

    void *prev_brk = sbrk((intptr_t)total_req);
    if (prev_brk == (void *)-1) {
        return NULL;
    }

    total_heap_bytes += total_req;

    block_t *b = (block_t *)((char *)prev_brk + pad);
    b->size = payload_size;
    b->next = NULL;
    b->free = 1;
    b->magic = BLOCK_MAGIC;

    if (head == NULL) {
        head = b;
        tail = b;
    } else {
        tail->next = b;
        tail = b;
    }

    return b;
}

void *my_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t need = align_up(size);
    block_t *curr = head;

    while (curr != NULL) {
        if (curr->free && curr->size >= need) {
            break;
        }
        curr = curr->next;
    }

    if (curr == NULL) {
        curr = grow(need);
        if (curr == NULL) {
            return NULL;
        }
    }

    if (curr->size >= need + HDR + 16) {
        block_t *rem = (block_t *)((char *)payload(curr) + need);
        rem->size = curr->size - need - HDR;
        rem->next = curr->next;
        rem->free = 1;
        rem->magic = BLOCK_MAGIC;

        curr->size = need;
        curr->next = rem;
        if (curr == tail) {
            tail = rem;
        }
    }

    curr->free = 0;
    return payload(curr);
}

size_t heap_bytes(void)
{
    return total_heap_bytes;
}

size_t free_bytes(void)
{
    size_t total = 0;
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->free) {
            total += curr->size;
        }
    }
    return total;
}
