# Placement Policy Experiment and Report

**Name:** Caroline Mueni  
**Registration Number:** SCT212-0046/2024  

## 1. Benchmark Results

Both placement policies were tested on the exact same workload of 50,000 random allocations and frees (block sizes between 1 and 4096 bytes, seeded with `23052026`), and timed alongside the standard C library (`glibc`) allocator using `clock_gettime()`.

| Allocator / Policy | Time (ms) | `heap_bytes` | `free_bytes` | `free_blocks` | `largest_free` | External Fragmentation ($1 - \frac{\text{largest\_free}}{\text{free\_bytes}}$) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`FIT_FIRST`** | 559.235 | 1,376,928 | 314,856 | 214 | 174,448 | 0.4456 |
| **`FIT_BEST`** | 1,845.008 | 1,311,360 | 249,984 | 160 | 107,600 | 0.5693 |
| **`glibc` (`malloc`/`free`)** | 48.274 | — | — | — | — | — |

## 2. Policy Choice and Comparison with `glibc`

Based on these results, I would choose **`FIT_FIRST`** for general workloads. Even though `FIT_BEST` used slightly less total heap memory from the OS (`1,311,360` bytes compared to `1,376,928` bytes), `FIT_FIRST` ran more than three times faster (`559.235 ms` vs. `1,845.008 ms`). First-fit stops searching as soon as it finds a block that is big enough, whereas best-fit has to scan the entire list on every request to find the tightest hole, leaving behind tiny leftover fragments that actually pushed external fragmentation higher (`0.5693` vs. `0.4456`).

My allocator is much slower than `glibc`'s `malloc`/`free` (`48.274 ms`) because `myalloc` keeps every allocated and free block in a single linked list, requiring a linear $O(n)$ scan during allocation and list traversal during `my_free()`. By contrast, `glibc` groups free chunks into size-specific bins and per-thread caches (`tcache`) for near-constant-time allocations, and uses boundary tags so adjacent free blocks can be merged in $O(1)$ time.
