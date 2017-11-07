/*
 * This implementation of malloc uses segregated explicit linked-lists.
 * The head of each linked list is maintained at the top of head.
 * The structure of an allocated block is shown below. A free block is similar.
 * -------------------------------------------------------------------
 * | header || prev_ptr | next_ptr ||...payload...| padding || footer |
 * -------------------------------------------------------------------
 *  HDRP(p)    p-DSIZE    p-WSIZE    p                        FTRP(p)
 *
 * Malloc attempts to find a fit in O(1). This is done by checking a constant
 * number of blocks in the same size linked-list, and then checking in larger
 * sized linked-list for a quick fit (rather than the best fit).
 * Free does immediate coalescing, and adds the node to the appropriate list.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

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
#define CHUNKSIZE   (1 << 6)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define GET_PTR(p)      (*(uintptr_t **)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))
#define PUT_PTR(p,ptr)  (*(uintptr_t **)(p) = (ptr))

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

/* Forward Declare mm_check since it was not done in header */
int mm_check();

const int kListSizes[24] = { 16, 32, 48, 64, 96, 128, 144, 160, 256, 512,
                             1024, 2048, 4096, 8192, 1 << 14, 1 << 15, 1 << 16,
                             1 << 17, 1 << 18, 1 << 20, 1 << 22, 1 << 31 };

const int kLength = sizeof(kListSizes) / sizeof(kListSizes[0]);

/**********************************************************
 * print_segregated_list
 * Helper function that prints out the state of the linked
 * list
 **********************************************************/
void print_segregated_list(void) {
    for (int i = 0; i < kLength; ++i) {
        uintptr_t* cur = GET_PTR(mem_heap_lo() + i);
        printf("%d: ->", kListSizes[i]);
        while (cur != NULL) {
            // Print out the size, pointer to prev, current address, and next
            printf("%lu (%p,%p,%p) -> ",
                   GET_SIZE(HDRP(cur)),
                   GET_PTR(GET_PREV(cur)),
                   cur,
                   GET_PTR(GET_NEXT(cur)));
            cur = GET_PTR(GET_NEXT(cur));
        }
        printf("\n");
    }
}

/**********************************************************
 * get_appropriate_list
 * Find the linked-list that is appropriate to insert the
 * free block
 **********************************************************/
int get_appropriate_list(size_t asize) {
    for (int i = 0; i < kLength; ++i)
        if (kListSizes[i] >= asize)
            return i;
    return -1;
}

/**********************************************************
 * get_possible_list
 * Find the smallest linked-list that has a free block that
 * can DEFINITELY fit asize. Runtime O(1)
 **********************************************************/
void* get_possible_list(size_t asize) {
    int i;
    uintptr_t* cur = NULL;
    for (i = 0; i < kLength; ++i) {
        if (kListSizes[i] >= asize && GET_PTR((uintptr_t *)mem_heap_lo() + i) != NULL) {
            cur = GET_PTR((uintptr_t *)mem_heap_lo() + i);
            if (GET_SIZE(HDRP(cur)) >= asize)
                return (void *)cur;
            else
                break;
        }
    }
    for (i = 0; i < kLength; ++i) {
        if (kListSizes[i] >= (asize << 1) && GET_PTR((uintptr_t *)mem_heap_lo() + i) != NULL) {
            cur = GET_PTR((uintptr_t *)mem_heap_lo() + i); 
            return (void *)cur;
        }
    }
    return NULL;
}

/**********************************************************
 * add_to_list
 * Adds a freed block to the linked-list.
 **********************************************************/
void add_to_list(void* p) {
    int list_number = get_appropriate_list(GET_SIZE(HDRP(p)));
    /* Check to see if the linked-list is empty (head is null) */
    uintptr_t* head = GET_PTR((uintptr_t *)mem_heap_lo() + list_number);
    if (head != NULL) {
        /* Set the next node to have its previous point here */
        PUT_PTR(GET_PREV(head), p);
    } 
    /* Point to the previous head of list */
    PUT_PTR(GET_NEXT(p), GET_PTR((uintptr_t *)mem_heap_lo() + list_number));
    PUT_PTR(GET_PREV(p), NULL);

    /* Update head of list */
    PUT_PTR((uintptr_t *)mem_heap_lo() + list_number, p);
}

/**********************************************************
 * free_from_list
 * Remove an allocated block from the linked-list.
 **********************************************************/
void free_from_list(void* p) { 
    int list_number = get_appropriate_list(GET_SIZE(HDRP(p)));
    /* If it is at the head, we must change the head */
    if (GET_PTR((uintptr_t *)mem_heap_lo() + list_number) == p) {
        PUT_PTR((uintptr_t *)mem_heap_lo() + list_number, GET_PTR(GET_NEXT(p)));
        if (GET_PTR(GET_NEXT(p)) != NULL) {
            PUT_PTR(GET_PREV(GET_PTR(GET_NEXT(p))), NULL); 
        }
    } else {
        PUT_PTR(GET_NEXT(GET_PTR(GET_PREV(p))), GET_PTR(GET_NEXT(p)));
        if (GET_PTR(GET_NEXT(p)) != NULL ) {
            PUT_PTR(GET_PREV(GET_PTR(GET_NEXT(p))), GET_PTR(GET_PREV(p)));
        }
    }
    PUT_PTR(GET_NEXT(p), NULL);
    PUT_PTR(GET_PREV(p), NULL);
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
	void* heap_listp = NULL;
    if ((heap_listp = mem_sbrk(4 * WSIZE + allocate_size + DSIZE)) == (void *)-1)
        return -1;
 
    for (int i = 0; i < kLength + 1; ++i) {
        PUT_PTR((uintptr_t*)heap_listp + i, NULL);    // Set the initial values to NULL
    }
    heap_listp += allocate_size;
    PUT(heap_listp + (0 * WSIZE ), 0);
    PUT(heap_listp + (1 * WSIZE ), PACK(DSIZE * 2, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE + DSIZE), PACK(DSIZE * 2, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE + DSIZE), PACK(0, 1));    // epilogue header
    heap_listp += DSIZE + DSIZE;
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
        add_to_list(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        int next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        /* Remove the next block from the appropriate ll */
        free_from_list(NEXT_BLKP(bp));
        size += next_size;
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        /* Add the current block, with newly updated size, to the app ll */
        add_to_list(bp);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        int prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        /* Remove the previous block from the appropriate ll */
        free_from_list(PREV_BLKP(bp));
        size += prev_size;
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        /* Add previous block, with newly updated size, to the app ll */
        add_to_list(PREV_BLKP(bp));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        /* Remove next and prev block from their appropriate ll */
        int next_size = GET_SIZE(FTRP(NEXT_BLKP(bp)));
        int prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
        free_from_list(PREV_BLKP(bp));
        free_from_list(NEXT_BLKP(bp));
        size += next_size + prev_size;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));

        // Add previous block, with newly updated size, to the appropriate ll
        add_to_list(PREV_BLKP(bp));
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

	void* last_blk_ft = mem_heap_hi() + 1 - DSIZE;
	void* last_blk_hd = last_blk_ft - GET_SIZE(last_blk_ft) + WSIZE;
    if (!GET_ALLOC(last_blk_hd)) {
      if (size <= GET_SIZE(last_blk_hd)) {
        return last_blk_hd + DSIZE + WSIZE;
      } else {
        /* The last block in the heap is free, and can be used to coalesce with
           the to-be-allocated block */
        size -= GET_SIZE(last_blk_hd);
      }
    }

    if ((bp = mem_sbrk(size)) == (void *)-1)
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
    size_t bsize = GET_SIZE(HDRP(bp));
    free_from_list(bp);
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
* separate_if_applicable
* Given a block pointer (bp) and a size_t (asize) that is
* guaranteed to fit within, split the block into two if
* able to.
**********************************************************/
void* separate_if_applicable(void* bp, size_t asize) {
	void* hdr_addr = HDRP(bp);
	size_t bsize = GET_SIZE(hdr_addr);
    /* Overhead consists of two pointer, header, and footer (4 * WSIZE) */
	if (bsize > asize + (WSIZE << 2)) {
		free_from_list(bp);
		size_t csize = bsize - asize;

		// First block allocated
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));

		// Second block unallocated
		PUT(HDRP(NEXT_BLKP(bp)), PACK(csize, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(csize, 0));
		add_to_list(NEXT_BLKP(bp));
		return bp;
	} else if (bsize >= asize) {
		free_from_list(bp);
		PUT(HDRP(bp), PACK(bsize, 1));
		PUT(FTRP(bp), PACK(bsize, 1));
		return bp;
	}
	return NULL;
}

/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void* find_fit(size_t asize)
{
    /* Determine the minimum size block we need, and pick the segregated list
       to check */
    void *bp = get_possible_list(asize);
    if (bp == NULL) {
        return NULL;
    }
	return separate_if_applicable(bp, asize);
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if (bp == NULL){
      return;
    }
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

/*********************************************************
 * get_adjusted_size adjusts the block size to account for
 * overhead, pointers, and alignment
 *********************************************************/
size_t get_adjusted_size(size_t size) {
    size_t asize;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    /* Add the following extra DSIZE for linked list structure */
    asize += DSIZE;
    return asize;
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
        mm_check();
    } 
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    asize = get_adjusted_size(size);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) { 
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
    if (size == 0){
        mm_free(ptr);
        return NULL;
    }
    /* If ptr is NULL, then this is just malloc. */
    if (ptr == NULL) {
        return (mm_malloc(size));
    }
    void *newptr;
    size_t cur_size;
    size_t asize;

    cur_size = GET_SIZE(HDRP(ptr));
    asize = get_adjusted_size(size);  

	/* If the desired size is less than the current size, then simply return.
	   For a general implementation of malloc, it might make sense to divide the
	   resulting block up. For the testcases here however, it reduces utilization. */
    if (asize < cur_size) {
        return ptr;
    }
    /* Check to see if there is room (free block) in front of the block */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    if (!next_alloc) {
        void* next_block = NEXT_BLKP(ptr); 
        if (cur_size + next_size >= asize) {
            /* Use the room since it is sufficient */
            free_from_list(next_block);
            PUT(HDRP(ptr), PACK(cur_size + next_size, 1));
            PUT(FTRP(ptr), PACK(cur_size + next_size, 1));
            return ptr; 
        }
    }

    /* If the given block that you want to extend is at the end of the heap,
       then extend the heap by the minimal amount */
    void* last_blk_ft = mem_heap_hi() + 1 - DSIZE;
    void* last_blk_hd = last_blk_ft - GET_SIZE(last_blk_ft) + WSIZE;

    if (HDRP(ptr) == last_blk_hd) {
        /* Only extend heap by the subtracted amount */
        size_t extendsize = asize - GET_SIZE(last_blk_hd); 
        if ((newptr = mem_sbrk(extendsize)) == (void *)-1)
            return NULL;

        /* Jump over pointers */
        newptr += DSIZE;

        /* Initialize free block header/footer and the epilogue header */
        PUT(HDRP(newptr), PACK(extendsize, 0));         // free block header
        PUT(FTRP(newptr), PACK(extendsize, 0));         // free block footer
        PUT(HDRP(NEXT_BLKP(newptr)), PACK(0, 1));       // new epilogue header
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(GET_SIZE(last_blk_hd), 1));
        return ptr;
    }

    /* Find a new block for fit and copy over data */
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    cur_size = GET_SIZE(HDRP(ptr));
    if (size < cur_size)
        cur_size = size;

    memcpy(newptr, ptr, cur_size);
    mm_free(ptr);
    return newptr;
}

/**********************************************************
 * check_implicitly
 * Check the correctness of the heap with a linear traversal
 * 1. Do any allocated blocks overlap? Note that is they do,
 *    then a linear traveral would not arrive at the end of
 *    the heap
 * 2. Do all blocks have valid pointers? (NULL or between
 *    mem_heap_lo() and mem_heap_hi()
 **********************************************************/
int check_implicitly(void) {
    void* bp = mem_heap_lo() + WSIZE * kLength + DSIZE + DSIZE;
    for (; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        void* next = GET_PTR(GET_NEXT(bp));
        void* prev = GET_PTR(GET_PREV(bp));
        if (next != NULL && (next <= mem_heap_lo() || next >= mem_heap_hi())) {
            printf("Error: Invalid next pointer at %p\n", next);
            return 0;
        } else if (prev != NULL && (prev <= mem_heap_lo() || prev >= mem_heap_hi())) {
            printf("Error: Invalid prev pointer at %p\n", prev);
            return 0;
        }
    }
    if (bp - DSIZE != mem_heap_hi() + 1) {
        printf("Error: Linear traversal of blocks ended before the end of heap\n");
        return 0;
    }
    return 1;
}

/**********************************************************
 * check_explicitly
 * Check the correctness of the segregated lists (sll)
 * 1. is every block in the sll actually free?
 * 2. does this free block belong to this list?
 * 3. are there any free blocks not coalesced properly?
 *********************************************************/
int check_explicitly(){
    int i;
    int prev = 0;
    size_t size;
    for (i = 0; i < kLength; ++i) {
        uintptr_t* cur = GET_PTR((uintptr_t *)mem_heap_lo() + i);
        while(cur != NULL) {
            if (GET_ALLOC(HDRP(cur))) {
                printf("Error: Block %p is allocated but found in the SLL.\n",
                        cur);
                return 0;
            }
            size = GET_SIZE(HDRP(cur));
            if (size > kListSizes[i] || size <= prev) {
                printf("Error: Block %p of size %lu is incorrectly put into SLL %d\n",
                       cur, size, kListSizes[i]);
                return 0;
            }  
            if (!GET_ALLOC(HDRP(PREV_BLKP(cur))) || !GET_ALLOC(HDRP(NEXT_BLKP(cur)))) {
                printf("Error: Block %p was not properly coalesced.\n", cur);
                return 0;
            }
            cur = GET_PTR(GET_NEXT(cur));
        }
        prev = kListSizes[i];
    }
    return 1;
}


/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check() {
	return check_explicitly() || check_implicitly;
}
