/**
 * Macros used from the textbook.
 **/
#include <stdio.h>

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */

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

void Init(size_t size);
static void *coalesce(void *bp);
addrs_t Malloc(size_t size);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
addrs_t Put(any_t data, size_t size);
void Get(any_t return_data, addrs_t addr, size_t size);
void Free(addrs_t addr);
void print_heap_status(unsigned long tot_alloc_time,
                       unsigned long tot_free_time, int mem_size,
                       int reset);
void reset_stats();

void Init(size_t size)
{
  /* Create the initial empty heap */
  if (size <= 0)
  {
    total_failures++;
    return;
  }
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
  PUT_WORD(heap_listp, PACK(DSIZE, 1)); // prologue header
  PUT_WORD(heap_listp + WSIZE, PACK(DSIZE, 1));
  PUT_WORD(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  // PUT_WORD(heap_listp+(3*WSIZE), PACK(DSIZE, 1));
  heap_listp += DSIZE;
  // PUT_WORD(FTRP(heap_listp), PACK(DSIZE, 1));  // prologue footer
  // PUT_WORD(HDRP(max_heap_p), PACK(0, 1));      // epilogue
  size_t freebsize = asize - 16; // free block
  PUT_WORD(HDRP(NEXT_BLKP(heap_listp)),
           PACK(freebsize, 0)); // free block header
  PUT_WORD(FTRP(NEXT_BLKP(heap_listp)),
           PACK(freebsize, 0)); // free block footer
  heap_listp = NEXT_BLKP(heap_listp);
}

static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  if (prev_alloc && next_alloc)
  { /* Case 1 */
    blocks_free++;
    return bp;
  }
  else if (prev_alloc && !next_alloc)
  { /* Case 2 */
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT_WORD(HDRP(bp), PACK(size, 0));
    PUT_WORD(FTRP(bp), PACK(size, 0));
  }
  else if (!prev_alloc && next_alloc)
  { /* Case 3 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT_WORD(FTRP(bp), PACK(size, 0));
    PUT_WORD(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  else
  { /* Case 4 */
    blocks_free--;
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT_WORD(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT_WORD(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  return bp;
}

addrs_t Malloc(size_t size)
{
  total_malloc_calls++;
  size_t asize; // Adjusted block size
  addrs_t bp;
  // Ignore spurious requests
  if (size <= 0)
  {
    total_failures++;
    return NULL;
  }
  if (!heap_listp)
  {
    Init(size);
  }
  // Adjust block size to include overhead and alignment reqs.
  if (size <= 2 * DSIZE)
    asize = 2 * DSIZE; // Allocate atleast 16 bytes for alignment
  else
    asize =
        DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // Align to 8 bytes

  // Search the free list for a fit
  if ((bp = find_fit(asize)) != NULL)
  {
    blocks_allocated++;
    raw_allocated += size;
    padded_allocated += asize - DSIZE;
    place(bp, asize);
    return bp;
  }
  total_failures++;
  return NULL;
}

static void *find_fit(size_t asize)
{
  void *bp;
  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      return bp; // First fit
    }
  }
  return NULL; // No fit
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
    blocks_free--;
  }
}

addrs_t Put(any_t data, size_t size)
{
  addrs_t bp = Malloc(size);
  if (bp != NULL)
  {
    memmove(bp, data, size);
  }
  return bp;
}

void Get(any_t return_data, addrs_t addr, size_t size)
{
  if (!addr)
  {
    return;
  }
  memmove(return_data, addr, size);
  Free(addr);
  raw_allocated -= size;
}

void Free(addrs_t addr)
{
  total_free_calls++;
  if (!addr)
  {
    total_failures++;
    return;
  }
  size_t size = GET_SIZE(HDRP(addr));
  PUT_WORD(HDRP(addr), PACK(size, 0));
  PUT_WORD(FTRP(addr), PACK(size, 0));
  coalesce(addr);
  blocks_allocated--;
  padded_allocated -= (size - DSIZE);
}

void print_heap_status(unsigned long tot_alloc_time,
                       unsigned long tot_free_time, int mem_size, int reset)
{
  printf("\n<<Part 1 for Region M1>>\n");
  printf("Number of allocated blocks: %ld\n", blocks_allocated);
  printf("Number of free blocks: %ld\n", blocks_free);
  printf("Raw total number of bytes allocated: %d\n", raw_allocated);
  printf("Padded total number of bytes allocated: %d\n", padded_allocated);
  raw_free = mem_size - (12) - raw_allocated;
  printf("Raw total number of bytes free: %d\n", raw_free);
  aligned_free = mem_size - (12) - padded_allocated;
  printf("Aligned total number of bytes free: %d\n", aligned_free);
  printf("Total number of Malloc requests: %d\n", total_malloc_calls);
  printf("Total number of Free requests: %d\n", total_free_calls);
  printf("Total number of request failures: %d\n", total_failures);
  printf("Average clock cycles for a Malloc request: %d\n",
         tot_alloc_time / (total_malloc_calls + 0.00001));
  int x;
  if (total_free_calls == 0)
  { // divide by 0 error
    x = 0;
  }
  else
  {
    x = tot_free_time / total_free_calls;
  }
  printf("Average clock cycles for a Free request: %d\n", x);
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