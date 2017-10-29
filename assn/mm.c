/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Charlie and Michelle",
    /* First member's full name */
    "Lei, Mei Siu",
    /* First member's email address */
    "michelle.lei@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Haoen Huang",
    /* Second member's email address (leave blank if none) */
    "haoen.huang@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define DEBUG 1

const int kListSizes[12] = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
int* ll_head = NULL;
const int kLength = sizeof(kListSizes) / sizeof(kListSizes[0]);
const int kOverhead = 16; // bytes
void* heap_listp = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{ 
    // We need to allocate room for kLength pointers
    int allocate_size = WSIZE * kLength;

    if ((heap_listp = mem_sbrk(4*WSIZE + allocate_size)) == (void *)-1)
        return -1;

    ll_head = heap_listp;
    //PUT(heap_listp, 0);                         // alignment padding
    int** p = heap_listp;
    for (int i = 0; i < kLength; ++i) {
        //PUT_NULL(heap_listp + i * WSIZE);
        *(p + i) = NULL;
    }
    PUT(heap_listp + (0 * WSIZE + allocate_size), 0);
    PUT(heap_listp + (1 * WSIZE + allocate_size), PACK(DSIZE, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE + allocate_size), PACK(DSIZE, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE + allocate_size), PACK(0, 1));    // epilogue header
    
    heap_listp += DSIZE + allocate_size;
    return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void* find_fit(size_t asize)
{
    void *bp;

    // Determine the minimum size block we need, and pick the segregated list
    // to check
    int list_choice = 12;
    for (int i = 0; i < kLength; ++i) {
        if (kListSizes[i] >= asize) {
            list_choice = i;
            break;
        }
    }
    if (DEBUG) {
        printf("Needs: %d room, Picking: %d\n", asize, list_choice);
    }
    // TODO: Look through the linked list, find the first set of unallocated blocks

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        void *hdr_addr = HDRP(bp);
        size_t bsize = GET_SIZE(hdr_addr);
        if (!GET_ALLOC(hdr_addr))
        {
            //only worth splitting if bsize is great than asize more than memory needed for header and footer
            if (bsize > asize + DSIZE) {
                size_t csize = bsize - asize ;
                //second-part unallocated
                PUT(hdr_addr+asize, PACK(csize, 0));
                PUT(FTRP(bp), PACK(csize, 0));
                //first-half allocated
                PUT(hdr_addr, PACK(asize, 1));
                PUT(hdr_addr+asize-WSIZE, PACK(asize, 1));
                return bp;
            }
            else if (asize == bsize) {
                PUT(hdr_addr, PACK(bsize, 1));
                PUT(FTRP(bp), PACK(bsize, 1));
                return bp;
            }
        }
    }
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
    /* Get the current block size */
    size_t bsize = GET_SIZE(HDRP(bp));

    // TODO: add prev and next
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{   
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    // Add the following extra DSIZE for linked list structure
    asize += DSIZE;

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        //place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  return 1;
}
