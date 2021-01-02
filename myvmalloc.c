/**
 * Macros used from the textbook.
 **/
#include <stdio.h>

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET_WORD(p) (*(unsigned int *)(p))
#define PUT_WORD(p, val) (*(unsigned int *)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET_WORD(p) & ~0x7)
#define GET_ALLOC(p) (GET_WORD(p) & 0x1)
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

typedef char *addrs_t;
typedef void *any_t;

addrs_t heap_listp;
addrs_t free_chunk;
addrs_t *RT = NULL;
static int RT_size = 0;

int blocks_allocated = 0;
int blocks_free = 1;
int raw_allocated = 0;
int padded_allocated = 0;
int raw_free = 0;
int aligned_free = 0;
int total_malloc_calls = 0;
int total_free_calls = 0;
int total_failures = 0;
int avg_malloc = 0;
int avg_free = 0;
int tot_clock = 0;

void VInit(size_t size);
addrs_t *VMalloc(size_t size);
void VFree(addrs_t *addr);
addrs_t *VPut(any_t data, size_t size);
void VGet(any_t return_data, addrs_t *addr, size_t size);
int find_RT_fit();
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
void print_heap_status(unsigned long tot_alloc_time,
                       unsigned long tot_free_time, int mem_size, int reset);
void reset_stats();

void VInit(size_t size)
{
  size_t asize;
  if (size % 8 != 0)
  {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  else
  {
    asize = size;
  }
  heap_listp = (addrs_t)malloc(asize);
  addrs_t max_heap_p = heap_listp + size;
  // heap_listp += DSIZE;
  PUT_WORD(heap_listp, PACK(DSIZE, 1));         // prologue header
  PUT_WORD(heap_listp + WSIZE, PACK(DSIZE, 1)); // prologue footer
  PUT_WORD(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT_WORD(heap_listp + (3 * WSIZE), PACK(DSIZE, 1));
  heap_listp += DSIZE;
  PUT_WORD(HDRP(max_heap_p), PACK(0, 1)); // epilogue
  size_t freebsize = asize - 16;          // free block
  PUT_WORD(HDRP(NEXT_BLKP(heap_listp)),
           PACK(freebsize, 0)); // free block header
  PUT_WORD(FTRP(NEXT_BLKP(heap_listp)),
           PACK(freebsize, 0)); // free block footer
  heap_listp = NEXT_BLKP(heap_listp);
  RT_size =
      (freebsize) / (2 * DSIZE); // The max number of possible allocatments
                                 // without including the boundaries;
  RT = malloc(RT_size * sizeof(addrs_t *));
  free_chunk = heap_listp;
}

int find_RT_fit()
{
  int i;
  for (i = 0; i < RT_size; i++)
  {
    if (RT[i] == NULL)
    {
      return i;
    }
  }
  return -1;
}

addrs_t *VMalloc(size_t size)
{
  total_malloc_calls++;
  size_t asize; // Adjusted block size
  addrs_t bp;
  int index;
  index = find_RT_fit();
  if (index == -1)
  {
    total_failures++;
    return NULL; // No space
  }
  // Ignore spurious requests
  if (size <= 0)
  {
    total_failures++;
    return NULL;
  }

  if (!heap_listp)
  {
    VInit(size);
  }

  // Adjust block size to include overhead and alignment reqs.
  if (size <= 2 * DSIZE)
    asize = 2 * DSIZE; // Allocate atleast 16 bytes for alignment
  else
    asize =
        DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // Align to 8 bytes

  // Search the free list for a fit
  if (GET_SIZE(HDRP(free_chunk)) >= asize &&
      !GET_ALLOC(HDRP(free_chunk)))
  { // space is left
    bp = free_chunk;
    place(bp, asize);
    blocks_allocated++;
    raw_allocated += size;
    padded_allocated += (asize - DSIZE);
    RT[index] = (addrs_t)bp;
    return &RT[index]; // pointer to data
  }
  total_failures++;
  return NULL;
}

void VFree(addrs_t *addr)
{
  total_free_calls++;
  if (addr == NULL || *addr == NULL)
  { // Spurious requests
    total_failures++;
    return;
  }
  addrs_t temp = *addr;
  addrs_t next = NEXT_BLKP(temp);
  size_t size = GET_SIZE(HDRP(temp));
  PUT_WORD(HDRP(temp), PACK(size, 0));
  PUT_WORD(FTRP(temp), PACK(size, 0));
  while (GET_ALLOC(HDRP(next)) &&
         GET_SIZE(HDRP(next)))
  { // while we need to move the allocations
    size_t next_size = GET_SIZE(HDRP(next));
    PUT_WORD(HDRP(temp), PACK(next_size, 1));
    PUT_WORD(FTRP(temp), PACK(next_size, 1));
    memmove(temp, next, next_size);
    next = NEXT_BLKP(temp);
    PUT_WORD(HDRP(next), PACK(size, 0));
    PUT_WORD(FTRP(next), PACK(size, 0));
    temp = next;
    next = NEXT_BLKP(next);
  }
  *addr = NULL; // We need to set to NULL
  blocks_allocated--;
  padded_allocated -= size - DSIZE;
  coalesce(free_chunk);
}

addrs_t *VPut(any_t data, size_t size)
{
  addrs_t *bp = VMalloc(size);
  if (bp != NULL)
  {
    memmove(*bp, data, size);
  }
  return bp;
}

void VGet(any_t return_data, addrs_t *addr, size_t size)
{
  memmove(return_data, *addr, size);
  VFree(addr);
  raw_allocated -= size;
}

static void *coalesce(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  size += GET_SIZE(HDRP(PREV_BLKP(bp)));
  PUT_WORD(FTRP(bp), PACK(size, 0));
  PUT_WORD(HDRP(PREV_BLKP(bp)), PACK(size, 0));
  bp = PREV_BLKP(bp);
  free_chunk = bp;
  return bp;
}

static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp)); // Get block size
  if (csize - asize >=
      (2 * DSIZE))
  { // Greater than 16 bytes, have to fragment
    PUT_WORD(HDRP(bp), PACK(asize, 1));
    PUT_WORD(FTRP(bp), PACK(asize, 1));
    PUT_WORD(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
    PUT_WORD(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
  }
  else
  {
    PUT_WORD(HDRP(bp), PACK(csize, 1));
    PUT_WORD(FTRP(bp), PACK(csize, 1));
  }
  free_chunk = NEXT_BLKP(bp);
}

void print_heap_status(unsigned long tot_alloc_time,
                       unsigned long tot_free_time, int mem_size, int reset)
{
  int x;
  printf("\n<<Part 2 for Region M2>>\n");
  printf("Number of allocated blocks: %ld\n", blocks_allocated);
  printf("Number of free blocks: %ld\n", blocks_free);
  printf("Raw total number of bytes allocated: %d\n", raw_allocated);
  printf("Padded total number of bytes allocated: %d\n", padded_allocated);
  raw_free = mem_size - (12) - raw_allocated;
  printf("Raw total number of bytes free: %d\n", raw_free);
  aligned_free = mem_size - (12) - padded_allocated;
  printf("Aligned total number of bytes free: %d\n", aligned_free);
  printf("Total number of VMalloc requests: %d\n", total_malloc_calls);
  printf("Total number of VFree requests: %d\n", total_free_calls);
  printf("Total number of request failures: %d\n", total_failures);
  printf("Average clock cycles for a VMalloc request: %d\n",
         tot_alloc_time / (total_malloc_calls + 0.0001));
  if (total_free_calls == 0)
  { // to avoid flaoting point exception we have to make
    // sure denominator is not zero
    x = 0;
  }
  else
  {
    x = tot_free_time / total_free_calls;
  }
  printf("Average clock cycles for a VFree request: %d\n", x);
  printf("Total clock cycles for all requests: %d\n",
         (tot_free_time + tot_alloc_time));
  if (reset)
    reset_stats(); // Reset values
}

void reset_stats()
{
  blocks_allocated = 0;
  blocks_free = 1;
  raw_allocated = 0;
  padded_allocated = 0;
  raw_free = 0;
  aligned_free = 0;
  total_malloc_calls = 0;
  total_free_calls = 0;
  total_failures = 0;
  avg_malloc = 0;
  avg_free = 0;
  tot_clock = 0;
}