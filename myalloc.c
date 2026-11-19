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
static fit_policy_t current_policy = FIT_FIRST;

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

void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST) {
        current_policy = p;
    }
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

static block_t *find_fit(size_t need)
{
    if (current_policy == FIT_BEST) {
        block_t *best = NULL;
        for (block_t *curr = head; curr != NULL; curr = curr->next) {
            if (curr->free && curr->size >= need) {
                if (best == NULL || curr->size < best->size) {
                    best = curr;
                    if (best->size == need) {
                        break;
                    }
                }
            }
        }
        return best;
    }

    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->free && curr->size >= need) {
            return curr;
        }
    }
    return NULL;
}

void *my_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t need = align_up(size);
    block_t *curr = find_fit(need);

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

static void coalesce_forward(block_t *b)
{
    while (b->next != NULL && b->next->free &&
           (char *)payload(b) + b->size == (char *)b->next) {
        block_t *nxt = b->next;
        b->size += HDR + nxt->size;
        b->next = nxt->next;
        nxt->magic = 0;
        if (nxt == tail) {
            tail = b;
        }
    }
}

void my_free(void *ptr)
{
    if (ptr == NULL || head == NULL) {
        return;
    }

    if ((uintptr_t)ptr % 16 != 0) {
        return;
    }

    block_t *b = header(ptr);
    if (b < head || b > tail) {
        return;
    }

    block_t *prev = NULL;
    block_t *curr = head;
    while (curr != NULL && curr != b) {
        prev = curr;
        curr = curr->next;
    }

    if (curr != b || b->magic != BLOCK_MAGIC || b->free) {
        return;
    }

    b->free = 1;
    coalesce_forward(b);

    if (prev != NULL && prev->free &&
        (char *)payload(prev) + prev->size == (char *)b) {
        coalesce_forward(prev);
    }
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }

    size_t total = nmemb * size;
    void *ptr = my_malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }
    if (head == NULL || (uintptr_t)ptr % 16 != 0) {
        return NULL;
    }

    block_t *b = header(ptr);
    if (b < head || b > tail || b->magic != BLOCK_MAGIC || b->free) {
        return NULL;
    }

    size_t need = align_up(size);

    if (b->size >= need) {
        if (b->size >= need + HDR + 16) {
            block_t *rem = (block_t *)((char *)payload(b) + need);
            rem->size = b->size - need - HDR;
            rem->next = b->next;
            rem->free = 1;
            rem->magic = BLOCK_MAGIC;

            b->size = need;
            b->next = rem;
            if (b == tail) {
                tail = rem;
            }
            coalesce_forward(rem);
        }
        return ptr;
    }

    if (b->next != NULL && b->next->free &&
        (char *)payload(b) + b->size == (char *)b->next &&
        b->size + HDR + b->next->size >= need) {
        block_t *nxt = b->next;
        b->size += HDR + nxt->size;
        b->next = nxt->next;
        nxt->magic = 0;
        if (nxt == tail) {
            tail = b;
        }

        if (b->size >= need + HDR + 16) {
            block_t *rem = (block_t *)((char *)payload(b) + need);
            rem->size = b->size - need - HDR;
            rem->next = b->next;
            rem->free = 1;
            rem->magic = BLOCK_MAGIC;

            b->size = need;
            b->next = rem;
            if (b == tail) {
                tail = rem;
            }
        }
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, ptr, b->size);
    my_free(ptr);
    return new_ptr;
}

void heap_stats(heap_stats_t *out)
{
    if (out == NULL) {
        return;
    }

    out->heap_bytes = total_heap_bytes;
    out->used_bytes = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;

    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        out->blocks++;
        if (curr->free) {
            out->free_bytes += curr->size;
            out->free_blocks++;
            if (curr->size > out->largest_free) {
                out->largest_free = curr->size;
            }
        } else {
            out->used_bytes += curr->size;
        }
    }
}

void heap_dump(void)
{
    heap_stats_t st;
    heap_stats(&st);

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
                     "=== HEAP DUMP (heap=%zu used=%zu free=%zu blocks=%zu free_blocks=%zu largest=%zu) ===\n",
                     st.heap_bytes, st.used_bytes, st.free_bytes,
                     st.blocks, st.free_blocks, st.largest_free);
    if (n > 0) {
        write(STDOUT_FILENO, buf, (size_t)n);
    }

    size_t idx = 0;
    for (block_t *curr = head; curr != NULL; curr = curr->next, idx++) {
        n = snprintf(buf, sizeof(buf),
                     "  [%zu] hdr=%p payload=%p size=%zu status=%s\n",
                     idx, (void *)curr, payload(curr), curr->size,
                     curr->free ? "FREE" : "USED");
        if (n > 0) {
            write(STDOUT_FILENO, buf, (size_t)n);
        }
    }

    const char *footer = "===================================================================================\n";
    write(STDOUT_FILENO, footer, strlen(footer));
}

int heap_check(void)
{
    if (head == NULL) {
        return (tail == NULL && total_heap_bytes == 0) ? 1 : 0;
    }

    void *brk = sbrk(0);
    block_t *prev = NULL;

    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->magic != BLOCK_MAGIC) {
            return 0;
        }
        if (curr->free != 0 && curr->free != 1) {
            return 0;
        }
        if (((uintptr_t)payload(curr) % 16) != 0) {
            return 0;
        }
        if (curr->size == 0 || (curr->size % 16) != 0) {
            return 0;
        }
        if (brk != (void *)-1 && (char *)payload(curr) + curr->size > (char *)brk) {
            return 0;
        }
        if (curr->next != NULL) {
            if (curr->next <= curr) {
                return 0;
            }
            if ((char *)payload(curr) + curr->size > (char *)curr->next) {
                return 0;
            }
            if (curr->free && curr->next->free &&
                (char *)payload(curr) + curr->size == (char *)curr->next) {
                return 0;
            }
        }
        prev = curr;
    }

    return (prev == tail) ? 1 : 0;
}
