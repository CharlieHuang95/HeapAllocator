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
//#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */
#define CHUNKSIZE   0      /* initial heap size (bytes) */

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
#define HDRP(bp)        ((char *)(bp) - WSIZE - DSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE - DSIZE)

/* Given block ptr bp, compute address of the prev and next in LinkedList */
#define GET_PREV(bp)    ((char *)(bp) - DSIZE)
#define GET_NEXT(bp)    ((char *)(bp) - WSIZE)


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE - DSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE - DSIZE)))

#define DEBUG 0

const int kListSizes[20] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192, 1 << 14,
                             1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19,
                             1 << 20, 1 << 21, 1 << 22, 1 << 23, 1 << 24,
                             1 << 25};

const int kLength = sizeof(kListSizes) / sizeof(kListSizes[0]);

// A pointer to the head of the segregated linked-list
uintptr_t* ll_head = NULL;
void* heap_listp = NULL;

void print_segregated_list(void) {
    for (int i = 0; i < kLength; ++i) {
        uintptr_t* cur = GET(ll_head + i);
        printf("%d: ->", kListSizes[i]);
        while (cur != -1) {
            // Print out the size, pointer to prev, current address, and next
            printf("%d (%d,%d,%d) -> ",
                   GET_SIZE(HDRP(cur)),
                   GET(GET_PREV(cur)),
                   cur,
                   GET(GET_NEXT(cur)));
            cur = GET(GET_NEXT(cur));
        }
        printf("\n");
    }
}

/* Find the smallest linked-list that is appropriate to insert the size */
int get_appropriate_list(size_t asize) {
    int list_choice = -1;
    for (int i = 0; i < kLength; ++i) {
        if (kListSizes[i] >= asize) {
            list_choice = i;
            break;
        }
    }
    return list_choice;
}

int get_possible_list(size_t asize) {
    int list_choice = -1;
    for (int i = 0; i < kLength; ++i) {
        if (kListSizes[i] >= asize * 2 && GET((ll_head + i)) != -1) {
            list_choice = i;
            break;
        }
    }
    return list_choice;
}

void add_to_list(void* p) {
    int list_number = get_appropriate_list(GET_SIZE(HDRP(p)));
    if (DEBUG) {
        printf("Adding a block of size %d.\n", GET_SIZE(HDRP(p)));
    }
    /* Check to see if the linked-list is empty (head is null) */
    uintptr_t* head = GET(ll_head + list_number);
    if (head != -1) {
        // Set the next node to have its previous point here
        PUT(GET_PREV(head), p);
    } 
    
    PUT(GET_NEXT(p), GET(ll_head + list_number));
    PUT(GET_PREV(p), -1);
    PUT(ll_head + list_number, p);
}

void free_from_list(void* p) {
    if (DEBUG) {
        printf("Removing a block of size %d\n", GET_SIZE(HDRP(p)));
    }
    int list_number = get_appropriate_list(GET_SIZE(HDRP(p)));
    /* If it is at the head, we must change the head */
    if (GET(ll_head + list_number) == p) {
        PUT(ll_head + list_number, GET(GET_NEXT(p)));
        if (GET(GET_NEXT(p)) != -1) {
            PUT(GET_PREV(GET(GET_NEXT(p))), -1);
        }
    } else {
        PUT(GET_NEXT(GET(GET_PREV(p))), GET(GET_NEXT(p)));
        if (GET(GET_NEXT(p)) != -1) {
            PUT(GET_PREV(GET(GET_NEXT(p))), GET(GET_PREV(p)));
        }
    }
    PUT(GET_NEXT(p), -1);
    PUT(GET_PREV(p), -1);
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{ 
    // We need to allocate room for kLength pointers
    int allocate_size = WSIZE * kLength;

    if ((heap_listp = mem_sbrk(4*WSIZE + allocate_size + DSIZE)) == (void *)-1)
        return -1;

    ll_head = heap_listp;
    int** p = heap_listp;
    for (int i = 0; i < kLength + 1; ++i) {
        PUT(p + i, -1);    // Set the initial values to NULL
    }
    PUT(heap_listp + (0 * WSIZE + allocate_size), 0);
    PUT(heap_listp + (1 * WSIZE + allocate_size), PACK(DSIZE * 2, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE + allocate_size + DSIZE), PACK(DSIZE * 2, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE + allocate_size + DSIZE), PACK(0, 1));    // epilogue header
    
    heap_listp += DSIZE + allocate_size + DSIZE;
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

    if (DEBUG) {
        printf("Attempting to coalesce: ");
    }

    if (prev_alloc && next_alloc) {       /* Case 1 */
        if (DEBUG) {
            printf("No coalescing possible\n");
        }
        add_to_list(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        if (DEBUG) {
            printf("About to combine with next\n");
        }
        int next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // Remove the next block from the appropriate ll
        free_from_list(NEXT_BLKP(bp));
        size += next_size;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        // Add the current block, with newly updated size, to the app ll
        add_to_list(bp);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        if (DEBUG) {
            printf("About to combine with previous\n");
        }
        int prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        // Remove the previous block from the appropriate ll
        free_from_list(PREV_BLKP(bp));
        size += prev_size;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // Add previous block, with newly updated size, to the app ll
        add_to_list(PREV_BLKP(bp));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        if (DEBUG) {
            printf("About to combine with previous and next\n");
        }
        // Remove next and prev block from their appropriate ll
        int next_size = GET_SIZE(FTRP(NEXT_BLKP(bp)));
        int prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        free_from_list(PREV_BLKP(bp));
        free_from_list(NEXT_BLKP(bp));
        size += next_size + prev_size;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        add_to_list(PREV_BLKP(bp));
        // Add previous block, with newly updated size, to the app ll
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
    if (DEBUG) {
        printf("Extending heap by %d\n", words * WSIZE);
    }
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;
    bp += DSIZE;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(GET_PREV(bp), -1);                       // set prev to NULL
    PUT(GET_NEXT(bp), -1);                       // set next to NULL
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    /* Let coalesce deal with the modification of the SLL */
    return coalesce(bp);
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
    if (DEBUG) {
        printf("About to allocate a block of size: %d\n", asize);
    }
    /* Get the current block size */
    size_t bsize = GET_SIZE(HDRP(bp));
    free_from_list(bp);
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));

    // TODO: Consider breaking the block
    void *hdr_addr = HDRP(bp);
}

/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void* find_fit(size_t asize)
{
    if (DEBUG) {
        printf("Attempting to find fit for %d\n", asize);
    }

    // Determine the minimum size block we need, and pick the segregated list
    // to check
    int list_choice = get_possible_list(asize);
    if (list_choice == -1) {
        return NULL;
    }
    if (DEBUG) {
        printf("Needs: %zu room, Picking from list: %d\n", asize, list_choice);
    }

    void* bp = GET(ll_head + list_choice);
    void* hdr_addr = HDRP(bp);
    size_t bsize = GET_SIZE(hdr_addr);
    if (bsize > asize + DSIZE + DSIZE) {
        if (DEBUG) {
            printf("Separated a block in %dth list, of size %d.\n",
                   kListSizes[list_choice], bsize);
        }
        // Remove the block from the free list
        void* tmp = GET(ll_head + list_choice);
        free_from_list(GET(ll_head + list_choice));
        size_t csize = bsize - asize;
        // second-part unallocated
        PUT(hdr_addr+asize, PACK(csize, 0));
        PUT(FTRP(tmp), PACK(csize, 0));
        // TODO: improve style
        add_to_list(hdr_addr+asize + DSIZE + WSIZE);
                
        // first-half allocated
        PUT(hdr_addr, PACK(asize, 1));
        PUT(hdr_addr+asize-WSIZE, PACK(asize, 1));
        return hdr_addr + WSIZE + DSIZE;
    } else if (asize == bsize) {
        free_from_list(GET(ll_head + list_choice));
        PUT(hdr_addr, PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
        return hdr_addr + WSIZE + DSIZE;
    }
    return NULL;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if (DEBUG) {
        printf("Free called\n");
    }
    if (bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
            
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);

    if (DEBUG) {
        printf("Finished Free of size %d:\n", size);
        print_segregated_list();
    }
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
    if (DEBUG) {
        printf("Malloc called for size %d\n", size);
    }
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
        if (DEBUG) {
            printf("Finished Malloc with a fit\n");
            print_segregated_list();
        }
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);

    //place(bp, asize);
    if (DEBUG) {
        printf("Finished Malloc:\n");
        print_segregated_list();
    }
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

    copySize = GET_SIZE(HDRP(oldptr));
    if (size + 48 < copySize) {
        return oldptr;
    }

    // TODO: check to see if there is space at the end

    // TODO: otherwise find new place for it
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
        // TODO: change location of end ptr, break off another piece if possible, return
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
