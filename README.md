# VirtualMemoryAllocator

A simple virtual memory allocator written in C.

This code contains an implicit free list based memory allocator based on the code provided by the labs and the textbook.

To compile the code run `$ make` from the shell.

You will be able to run `$ ./mymalloc` and `$ ./myvmalloc`.

Both `mymalloc.c` and `myvmalloc.c` contain the function `void print_heap_status(unsigned long tot_alloc_time, unsigned long tot_free_time, int mem_size, int reset);`.
This function can be used to record statistics for memory requests.

It can be called by: `print_heap_status(tot_alloc_time, tot_free_time, mem_size, 1);`. The last field allows us to toggle whether or not I wish to reset the current stats.
