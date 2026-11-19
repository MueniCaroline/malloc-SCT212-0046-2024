#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "myalloc.h"

#define BLOCK_MAGIC 0x4D414C4C

typedef struct block {
    size_t        size;    /* payload size in bytes             */
    struct block *next;    /* next block, in increasing address */
    int           free;    /* 1 = free, 0 = in use              */
    int           magic;   /* constant to detect bad pointers   */
} block_t;

#define ALIGN_UP(n) (((size_t)(n) + 15) & ~(size_t)15)
#define HDR         ALIGN_UP(sizeof(block_t))

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
