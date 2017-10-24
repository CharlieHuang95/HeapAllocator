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
#define CHUNKSIZE   (1<<12)      /* initial heap size (bytes) */

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
#define HDRP(bp)        ((char *)(bp) - 3*WSIZE)
#define PREP(bp)        ((char *)(bp) - DSIZE)
#define SUSP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - 2*DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - 3*WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - 2*DSIZE)))

void* heap_listp = NULL;

/* pointer to the first free block*/
void* free_list_hd = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                         // alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(2*DSIZE, 1));   // prologue header
    PUT((char *)(heap_listp + (2 * WSIZE)), NULL); //prologue predecessor
    PUT((char *)(heap_listp + (3 * WSIZE)), NULL);  //prologue successor
    PUT(heap_listp + (4 * WSIZE), PACK(2*DSIZE, 1));   // prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));    // epilogue header
    heap_listp += 2*DSIZE;
    /*printf("%p %p %p %lu %lu\n",  HDRP(heap_listp), heap_listp, FTRP(heap_listp), GET_SIZE(HDRP(heap_listp)), GET_ALLOC(HDRP(heap_listp)));
    fflush(stdout);*/
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
        /*printf("coalesce\n");
        printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
        fflush(stdout);*/
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        /*printf("coalesce\n");
         printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
         fflush(stdout);*/
        fflush(stdout);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        /*printf("coalesce\n");
         printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
         fflush(stdout);*/
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        /*printf("coalesce\n");
         printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
         fflush(stdout);*/
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
    /* the blocks are like this right now
        old_epilogue|bp |...|..........|...|
       want to turn into:
                 HDR|PRE|SUS|bp|.......|FTR|
       which means shifting bp to the right by two words will
       allow us to use the macros like HDRP, FTRP, etc. */
    bp += DSIZE;
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(PREP(bp), NULL);
    PUT(SUSP(bp), NULL);
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header
    /*printf("extending\n");
    printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
    fflush(stdout);*/
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        //printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
        fflush(stdout);
        void *hdr_addr = HDRP(bp);
        size_t bsize = GET_SIZE(hdr_addr);
        if (!GET_ALLOC(hdr_addr))
        {
            //only worth splitting if bsize is great than asize more than memory needed for header and footer
            if (bsize > asize + 2*DSIZE) {
                //Split
                size_t csize = bsize - asize ;
                //second-part unallocated and inherit original predecessor and successor
                PUT(hdr_addr+asize, PACK(csize, 0));
                PUT(hdr_addr+asize+WSIZE, GET(PREP(bp)));
                PUT(hdr_addr+asize+DSIZE, GET(SUSP(bp)));
                PUT(FTRP(bp), PACK(csize, 0));
                //first-half allocated
                PUT(hdr_addr, PACK(asize, 1));
                PUT(hdr_addr+asize-WSIZE, PACK(asize, 1));
                /*printf("split\n");
                printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
                fflush(stdout);*/
                return bp;
            }
            else if (asize == bsize) {
                PUT(hdr_addr, PACK(bsize, 1));
                PUT(FTRP(bp), PACK(bsize, 1));
                /*printf("no split\n");
                printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
                fflush(stdout);*/
                return bp;
            }
        }
    }
    return NULL;
}

//DEPRECATED
/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

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
        asize = 3 * DSIZE;
    else
        asize = DSIZE * ((size + (2*DSIZE) + (DSIZE-1))/ DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        //place(bp, asize);
        /*printf("here\n");
        fflush(stdout);*/
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    /*printf("there\n");
    fflush(stdout);*/
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    bp = find_fit(asize);
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
