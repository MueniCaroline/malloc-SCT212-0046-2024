# Custom Dynamic Memory Allocator (`myalloc`)

- **Name:** Caroline Mueni
- **Registration Number:** SCT212-0046/2024

## Build and Run Commands

    make && ./test_myalloc
    gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c && ./t_asan && rm -f t_asan
    gcc -std=c11 -O2 -Wall -Wextra -o bench bench.c myalloc.c && ./bench first && ./bench best && ./bench libc

## Task Completion Status

| Task | Description | Status |
| :--- | :--- | :---: |
| **Task 1** | Block header layout (`block_t`) and 16-byte alignment helpers | Complete |
| **Task 2** | Heap growth via `sbrk()` (`grow()`) with 64 KB minimum chunks | Complete |
| **Task 3** | `my_malloc()` with first-fit search and block splitting | Complete |
| **Task 4** | `my_free()` with safety checks and forward/backward coalescing | Complete |
| **Task 5** | `my_calloc()` (overflow guard) and `my_realloc()` (in-place & move) | Complete |
| **Task 6** | Heap diagnostics (`heap_stats()`, `heap_dump()`, `heap_check()`) | Complete |
| **Task 7** | Dynamic placement policies (`FIT_FIRST` and `FIT_BEST`) | Complete |
| **Task 8** | Benchmark harness (`bench.c`), report (`REPORT.md`), and written questions | Complete |

## Final Test Output (`make && ./test_myalloc`)

    T6 heap growth via sbrk
      PASS  large request succeeds
      PASS  heap grew
      PASS  heap consistent
    T7 best-fit vs first-fit
      PASS  first-fit takes the first hole
      PASS  best-fit takes the tightest hole
    T8 randomized stress test
      PASS  no data corruption during 20000 operations
      PASS  everything freed
      PASS  fully coalesced (few free blocks remain)
      PASS  heap consistent

    Result: 29 passed, 0 failed

## Design Explanation

Each heap block begins with a 32-byte metadata header (`block_t`) storing the payload size (`size`), a pointer to the next block in ascending address order (`next`), an allocation status flag (`free`), and a magic constant (`0x4D414C4C`) for pointer validation. Because `sizeof(block_t)` is rounded up to a 16-byte multiple (`HDR = 32`) and initial `sbrk()` breaks are aligned to 16 bytes, every returned user payload pointer `(char *)block + HDR` is guaranteed to be 16-byte aligned.

All allocated and free blocks live in a single address-ordered linked list bounded by `head` and `tail`. On `my_malloc(size)`, the requested size is rounded up via `align_up()`, and `find_fit()` selects either the first suitable free block (`FIT_FIRST`) or the tightest suitable free block (`FIT_BEST`). If no free block fits, `grow()` requests `max(need, 64 KB)` from the operating system via `sbrk()`, and whenever a chosen block has at least `HDR + 16` spare bytes after satisfying `need`, it is split into an allocated prefix and a new free remainder block.

On `my_free(ptr)`, the allocator checks alignment, heap bounds, list membership, magic number, and double-free status before marking the block free. Coalescing then merges adjacent free neighbors both forward (`coalesce_forward()`) and backward (using the predecessor tracked during list traversal), verifying `(char *)payload(left) + left->size == (char *)right` so blocks separated by alignment padding across separate `sbrk()` calls are never merged across a gap.

## Questions

1. **Why must the header size be a multiple of 16 as well as the payload sizes?**  
   The user payload sits directly after the block header at `(char *)block + HDR`. If the header size were not a multiple of 16, adding `HDR` to a 16-byte-aligned block address would shift the payload off its 16-byte boundary. Keeping both the header size and every payload size aligned to 16 bytes ensures that every subsequent split block and payload also starts on a 16-byte boundary for strict C alignment and SIMD instructions.

2. **Why can `printf()` inside `my_malloc()` cause infinite recursion?**  
   Standard `stdio` functions like `printf()` allocate internal formatting buffers on the heap using `malloc()` when formatting or buffering output. If `my_malloc()` (or a `malloc` wrapper built on it) calls `printf()`, `printf()` calls right back into the allocator to get buffer space. That call triggers `printf()` again, creating an endless recursive loop that overflows the stack and crashes the process.

3. **Your `my_free()` is $O(n)$. Which extra field would let you find a block's previous neighbor in $O(1)$, and what is the name of the technique?**  
   Adding a `struct block *prev` pointer inside `block_t`—or placing a footer at the end of each block that duplicates its size and free flag—allows `my_free()` to reach the immediately preceding block in $O(1)$ time without scanning from `head`. Storing matching size and allocation status metadata at both the beginning (header) and end (footer) of each block is known as **boundary tags**.

4. **Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.**  
   **Internal fragmentation** is wasted space inside an allocated block that the program never asked for, whereas **external fragmentation** is free memory broken into scattered, non-contiguous holes that cannot satisfy a single large request even when total free memory is enough. In `myalloc`, internal fragmentation happens when `align_up()` rounds a small request (such as 1 byte) up to 16 bytes, or when a block is not split because the leftover space is smaller than `HDR + 16`. External fragmentation happens when freed blocks are separated by live allocated blocks in the list, preventing them from coalescing into one large contiguous region.

5. **Why does `sbrk()` memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.**  
   `my_free()` only sets `b->free = 1` and merges adjacent free nodes inside the user-space linked list without moving the program break downward. To return memory at the end of the heap, `my_free()` could check after coalescing whether `tail->free == 1` and `tail->size` exceeds a release threshold such as 64 KB. When it does, the allocator could unlink `tail` (or shrink its size) and call `sbrk(-(intptr_t)reclaim_bytes)` to lower the program break and hand those pages back to the kernel.

6. **Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?**  
   This happens if external code invokes `sbrk()` directly between two `grow()` calls, or when a later `grow()` call has to insert alignment padding bytes because the current program break is not aligned to 16 bytes. As a result, there is a gap of unowned or padding bytes between the end of `left`'s payload and the start of `right`'s header even though `left->next == right`. Before merging two free neighbors, our coalescing code checks `(char *)payload(left) + left->size == (char *)right`, skipping the merge whenever a byte gap exists so unowned memory is never absorbed into a block.

## Known Limitations & Declaration of Sources

- **Known Limitations:** Single-threaded only (no mutex synchronization), $O(n)$ linear block search and backward traversal rather than segregated free lists, and heap memory obtained via `sbrk()` is retained for reuse rather than shrunk back to the OS.
- **Declaration of Sources & AI Tools:** Implemented independently using the provided assignment starter pack (`myalloc.h`, `test_myalloc.c`, `Makefile`) and Linux `sbrk(2)` / `write(2)` manual pages. AI assistance (Gemini) was used for step-by-step verification, debugging header macro ordering, and structuring the benchmark harness and documentation.
