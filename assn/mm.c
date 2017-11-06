/*
 * This implementation of malloc uses segregated explicit linked lists.
 * The head of each linked list is maintained at the top of head.
 
 * Malloc attempts to find a fit in O(1). This is done by finding a free
 * larger block.
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
#define CHUNKSIZE   (1<<6)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

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
#define M_DEBUG 0

/* Forward Declare mm_check */
int mm_check(void);

const int kListSizes[26] = { 16, 32, 48, 64, 96, 128, 144, 160, 256, 512,
                             1024, 2048,
                             4096, 8192, 1 << 14, 1 << 15, 1 << 16,
                             1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21,
                             1 << 22, 1 << 23, 1 << 24, 1 << 25};

const int kLength = sizeof(kListSizes) / sizeof(kListSizes[0]);
void* heap_listp = NULL;


/**********************************************************
 * print_segregated_list
 * Helper function that prints out the state of the linked list
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
 * Find the linked-list that is appropriate to insert the free block
 **********************************************************/
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

/**********************************************************
 * get_possible_list
 * Find the smallest linked-list that has a free block that can DEFINITELY fit asize
 **********************************************************/
void* get_possible_list(size_t asize) {
  int i;
  uintptr_t* cur = NULL;
  for (i = 0; i < kLength; ++i) {
    if (kListSizes[i] >= asize && GET_PTR((uintptr_t *)mem_heap_lo() + i) != NULL) {
      cur = GET_PTR((uintptr_t *)mem_heap_lo() + i);
      if(GET_SIZE(HDRP(cur)) >= asize) {
        if (DEBUG) {
          printf("found cur same level up: %p\n", cur);
        }
        return (void *)cur;
      }
      else {
        break;
      }
    }
  }
  for (i = 0; i < kLength; ++i) {
    if (kListSizes[i] >= asize * 2 && GET_PTR((uintptr_t *)mem_heap_lo() + i) != NULL) {
      cur = GET_PTR((uintptr_t *)mem_heap_lo() + i);
      if (DEBUG) {
        printf("found cur elsewhere: %p\n", cur);
      }
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
	if (DEBUG) {
		printf("Adding a block of size %lu.\n", GET_SIZE(HDRP(p)));
	}
    int list_number = get_appropriate_list(GET_SIZE(HDRP(p)));
    /* Check to see if the linked-list is empty (head is null) */
    uintptr_t* head = GET_PTR((uintptr_t *)mem_heap_lo() + list_number);
    if (head != NULL) {
        // Set the next node to have its previous point here
        PUT_PTR(GET_PREV(head), p);
    } 
    
    PUT_PTR(GET_NEXT(p), GET_PTR((uintptr_t *)mem_heap_lo() + list_number));
    PUT_PTR(GET_PREV(p), NULL);
    PUT_PTR((uintptr_t *)mem_heap_lo() + list_number, p);
}

/**********************************************************
 * free_from_list
 * Remove an allocated block from the linked-list.
 **********************************************************/
void free_from_list(void* p) { 
	/* Verify that the given block is allocated, and thus should be removed from the linked-list */
    if (!GET_ALLOC(HDRP(p))) {
        //printf("Error: Attempting to remove a freed block.\n");
    }
    if (DEBUG) {
        printf("Removing a block of size %lu\n", GET_SIZE(HDRP(p)));
    }
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
    //uintptr_t* heap_listp;
    if ((heap_listp = mem_sbrk(4*WSIZE + allocate_size + DSIZE)) == (void *)-1)
        return -1;
 
    for (int i = 0; i < kLength + 1; ++i) {
        PUT_PTR((uintptr_t*)heap_listp + i, NULL);    // Set the initial values to NULL
    }
    PUT(heap_listp + (0 * WSIZE + allocate_size), 0);
    PUT(heap_listp + (1 * WSIZE + allocate_size), PACK(DSIZE * 2, 1));   // prologue header
    PUT(heap_listp + (2 * WSIZE + allocate_size + DSIZE), PACK(DSIZE * 2, 1));   // prologue footer
    PUT(heap_listp + (3 * WSIZE + allocate_size + DSIZE), PACK(0, 1));    // epilogue header
    heap_listp += DSIZE + allocate_size + DSIZE;
    if(DEBUG) {
      printf("init heap head %p\n", heap_listp);
    }
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
            printf("%p %p prev size:%lu prev alloc:%lu\n",
                 bp,
                 PREV_BLKP(bp),
                 GET_SIZE(HDRP(PREV_BLKP(bp))),
                 GET_ALLOC(FTRP(PREV_BLKP(bp))));
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
        printf("Extending heap by %lu\n", words * WSIZE);
        print_segregated_list();
    }
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

	void* heap_last_block_footer = mem_heap_hi() + 1 - DSIZE;
	void* heap_last_block_header = heap_last_block_footer - GET_SIZE(heap_last_block_footer) + WSIZE;
    if (GET_ALLOC(heap_last_block_header) == 0) {
      if (size <= GET_SIZE(heap_last_block_header)) {
        return heap_last_block_header + DSIZE + WSIZE;
      }
      else {
        //The last block in the heap is free so can be used to coalesce with the soon allocated block
        size -= GET_SIZE(heap_last_block_header);
      }
    }
    if (DEBUG) {
      printf("recalculated size: %lu\n", size);
    }
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
    void* new_bp = coalesce(bp);
    if (DEBUG) {
      print_segregated_list();
    }

    return new_bp;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
    if (DEBUG) {
        printf("About to allocate a block of size: %lu\n", asize);
    }
    /* Get the current block size */
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
	if (bsize > asize + DSIZE + DSIZE + /* buffer */ (DSIZE << 1)) {
		if (DEBUG) {
			printf("Separated a block of size %lu.\n", bsize);
		}
		// Remove the block from the free list
		free_from_list(bp);
		size_t csize = bsize - asize;
		// first-half allocated
		PUT(hdr_addr, PACK(asize, 1));
		PUT(hdr_addr + asize - WSIZE, PACK(asize, 1));
		// second-part unallocated
		PUT(hdr_addr + asize, PACK(csize, 0));
		PUT(hdr_addr + bsize - WSIZE, PACK(csize, 0));
		// TODO: improve style
		add_to_list(hdr_addr + asize + DSIZE + WSIZE);
		return bp;
	}
	else if (bsize >= asize) {
		free_from_list(bp);
		PUT(HDRP(bp), PACK(bsize, 1));
		PUT(FTRP(bp), PACK(bsize, 1));
		//return hdr_addr + WSIZE + DSIZE;
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
    if (DEBUG) {
        printf("Attempting to find fit for %lu\n", asize);
    }

    // Determine the minimum size block we need, and pick the segregated list
    // to check
    void *bp = get_possible_list(asize);
    if (bp == NULL) {
        if (DEBUG) {
          printf("find_fit did not find anything\n");
        }
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
    if (M_DEBUG) {
      mm_check();
    }
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
        printf("Finished Free of size %lu:\n", size);
        print_segregated_list();
    }
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
    // Add the following extra DSIZE for linked list structure
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
    if (M_DEBUG) {
      mm_check();
    }
    if (DEBUG) {
        printf("Malloc called for size %lu\n", size);
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
    if (M_DEBUG) {
      mm_check();
    }
    if(DEBUG){
        printf("realloc: alloc %lu old %lu, new %lu\n", GET_ALLOC(HDRP(ptr)), GET_SIZE(HDRP(ptr)), size);
    }
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
    size_t asize;

    copySize = GET_SIZE(HDRP(oldptr));
    asize = get_adjusted_size(size);    
    if (asize < copySize) {
        return ptr;
        add_to_list(ptr);
        return separate_if_applicable(ptr, asize);
    }
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    if (!next_alloc) {
        // Remove the next block from the linked-list
        void* next_block = NEXT_BLKP(ptr); 
        if (copySize + next_size >= asize) {
            free_from_list(next_block);
            PUT(HDRP(ptr), PACK(copySize + next_size, 1));
            PUT(FTRP(ptr), PACK(copySize + next_size, 1));
            return ptr;
            //add_to_list(ptr);
            //return separate_if_applicable(ptr, asize);
        }
        
    }
        

    // find new place for it
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
 * mm_check_sll
 * Check the correctness of the segregated lists (sll)
 * 1. is every block in the sll actuall free?
 * 2. does this free block belong to this list?
 * 3. are there any free blocks not coalesced properly?
 *********************************************************/
int mm_check_sll(){
  int i;
  int result = 1;
  int prev = 0;
  for (i = 0; i < kLength; ++i) {
    uintptr_t* cur = GET_PTR((uintptr_t *)mem_heap_lo() + i);
    while(cur!=NULL) {
      if (GET_ALLOC(HDRP(cur)) == 1) {
        printf("%p is allocated but found in the sll\n", cur);
        result = 0;
      }
      size_t size = GET_SIZE(HDRP(cur));
      if(size > kListSizes[i] || size <=prev) {
        printf("block %p of size %lu is incorrectly put into sll %d\n", cur, size, kListSizes[i]);
        result = 0;
      }
      char* prev_blk = PREV_BLKP(cur);
      char* nxt_blk = NEXT_BLKP(cur);
      if(GET_ALLOC(HDRP(prev_blk))==0 || GET_ALLOC(HDRP(nxt_blk))==0) {
        printf("block %p was not properly coalesced.\n", cur);
        result = 0;
      }
      cur = GET_PTR(GET_NEXT(cur));
    }
    prev = kListSizes[i];
  }
  return result;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  int result = 1;
  result = mm_check_sll();
  return result;
}
