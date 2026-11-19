#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "myalloc.h"

#define NUM_OPS   50000
#define MAX_SLOTS 1024
#define SEED      23052026u

static double elapsed_ms(struct timespec a, struct timespec b)
{
    double sec  = (double)(b.tv_sec - a.tv_sec) * 1000.0;
    double nsec = (double)(b.tv_nsec - a.tv_nsec) / 1e6;
    return sec + nsec;
}

static void run_custom(fit_policy_t policy, const char *label)
{
    void *slots[MAX_SLOTS] = {0};
    set_policy(policy);
    srand(SEED);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % MAX_SLOTS;
        if (slots[idx] != NULL) {
            my_free(slots[idx]);
            slots[idx] = NULL;
        } else {
            size_t sz = (size_t)(rand() % 4096) + 1;
            slots[idx] = my_malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    heap_stats_t st;
    heap_stats(&st);
    double frag = (st.free_bytes > 0)
        ? 1.0 - ((double)st.largest_free / (double)st.free_bytes)
        : 0.0;

    printf("%-10s | time_ms=%8.3f | heap_bytes=%8zu | free_bytes=%8zu | free_blocks=%5zu | largest_free=%8zu | ext_frag=%.4f\n",
           label, elapsed_ms(t0, t1), st.heap_bytes, st.free_bytes,
           st.free_blocks, st.largest_free, frag);

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i] != NULL) {
            my_free(slots[i]);
            slots[i] = NULL;
        }
    }
}

static void run_libc(void)
{
    void *slots[MAX_SLOTS] = {0};
    srand(SEED);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % MAX_SLOTS;
        if (slots[idx] != NULL) {
            free(slots[idx]);
            slots[idx] = NULL;
        } else {
            size_t sz = (size_t)(rand() % 4096) + 1;
            slots[idx] = malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("%-10s | time_ms=%8.3f | (glibc ptmalloc2 baseline)\n",
           "LIBC", elapsed_ms(t0, t1));

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i] != NULL) {
            free(slots[i]);
            slots[i] = NULL;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [first|best|libc]\n", argv[0]);
        return 1;
    }
    if (argv[1][0] == 'f') {
        run_custom(FIT_FIRST, "FIT_FIRST");
    } else if (argv[1][0] == 'b') {
        run_custom(FIT_BEST, "FIT_BEST");
    } else {
        run_libc();
    }
    return 0;
}
