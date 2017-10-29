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
#define HDRP(bp)        ((char *)(bp) - DSIZE)
#define PREP(bp)        ((char *)(bp) - WSIZE)
#define SUSP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - 2*DSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - 3*WSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - DSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - 3*WSIZE)))

void* heap_listp = NULL;

/* pointer to the first free block*/
void* free_list_hd = NULL;
void* free_list_tail = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue
 **********************************************************/
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(2*DSIZE)) == (void *)-1)
        return -1;
    PUT((char *)heap_listp, PACK(2*DSIZE, 1));   // prologue header
    PUT((char *)(heap_listp + WSIZE), 0); //prologue predecessor
    PUT((char *)(heap_listp + DSIZE), 0);  //prologue successor
    PUT((char *)heap_listp + (3*WSIZE), PACK(2*DSIZE, 1));   // prologue footer
    heap_listp += DSIZE;
    /*printf("%p %p %p %lu %lu\n",  HDRP(heap_listp), heap_listp, FTRP(heap_listp), GET_SIZE(HDRP(heap_listp)), GET_ALLOC(HDRP(heap_listp)));
    fflush(stdout);*/
    return 0;
}

int coalesce_both(void *bp) {
    void *right = NEXT_BLKP(bp);
    void *left = PREV_BLKP(bp);
    size_t asize = GET_SIZE(HDRP(left)) + GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(right));
    PUT(SUSP(left),GET(SUSP(right)));
    if (GET(SUSP(right)) == 0) {
        free_list_tail = left;
    }
    //Update size at last
    PUT(HDRP(left), PACK(asize,0));
    PUT(FTRP(right), PACK(asize,0));
    return 0;
}

int coalesce_right(void *bp) {
    //mm_check();
    char *right = NEXT_BLKP(bp);
    size_t asize = GET_SIZE(HDRP(bp)) + GET_SIZE(HDRP(right));
    PUT(PREP(bp), GET(PREP(right)));
    PUT(SUSP(bp), GET(SUSP(right)));
    if (GET(PREP(right)) != 0) {
        PUT(SUSP(GET(PREP(right))), bp);
    }
    else{
        PUT(PREP(bp),0);
        free_list_hd = bp;
    }
    if (GET(SUSP(right)) != 0) {
        PUT(PREP(GET(SUSP(right))), bp);
    }
    else {
        PUT(SUSP(bp),0);
        free_list_tail = bp;
    }
    //Update size at last
    PUT(HDRP(bp), PACK(asize, 0));
    PUT(FTRP(right), PACK(asize, 0));
    //mm_check();
    return 0;
}

int coalesce_left(void *bp) {
    char *left = PREV_BLKP(bp);
    size_t asize = GET_SIZE(HDRP(left)) + GET_SIZE(HDRP(bp));
    PUT(SUSP(bp), GET(SUSP(left)));
    //Update size at last
    PUT(HDRP(left), PACK(asize, 0));
    PUT(FTRP(bp), PACK(asize, 0));
    return 0;
}

int insert_middle(void *bp, void *right_free_block){
    PUT(SUSP(bp),right_free_block);
    PUT(PREP(bp),GET(PREP(right_free_block)));
    size_t asize = GET_SIZE(HDRP(bp));
    if (GET(PREP(right_free_block))!=0) {
        PUT(SUSP(GET(PREP(right_free_block))), bp);
    }
    else{
        PUT(PREP(bp),0);
        free_list_hd = bp;
    }
    PUT(PREP(right_free_block), bp);
    //Update size at last
    PUT(HDRP(bp),PACK(asize,0));
    PUT(FTRP(bp),PACK(asize,0));
    return 0;
}


/**********************************************************
 * coalesce_free_list
 * Covers the 4 cases discussed in the text:
 * 1. both neighbours are free blocks for coalescing
 * 2. the right neighbour is a free block for coalescing
 * 3. the left neighbour is free block for coalescing
 * 4. both neighbours are allocated
 * Two corner cases:
 * -inserting or coalescing a free block at the front of
 *      the free list.
 * -inserting or coalescing a free block at the end of
 *      the free list.
 **********************************************************/
int coalesce_free_list(void *bp, void *right_free_block)
{
    /*printf("coalesce_free_list\n");
    fflush(stdout);*/
    if ( NEXT_BLKP(bp) == right_free_block ){
        if (PREV_BLKP(bp) == GET(PREP(right_free_block))) {
            /* case 1 */
            printf("coalesce_both\n");
            fflush(stdout);
            coalesce_both(bp);
        }
        else {
            /* case 2 */
            printf("coalesce_right\n");
            fflush(stdout);
            coalesce_right(bp);
        }
    }
    else if (PREV_BLKP(bp) == GET(PREP(right_free_block))) {
        /* case 3 */
        printf("coalesce_left\n");
        fflush(stdout);
        coalesce_left(bp);
    }
    else {
        /* case 4 */
        printf("insert_middle\n");
        fflush(stdout);
        insert_middle(bp, right_free_block);
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
int extend_heap(size_t words)
{
    char *bp;
    size_t size;
    printf("extend heap before\n");
    fflush(stdout);
    mm_check();
    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return 1;
    //assert( mm_check_brk(bp)==0);
    /* Initialize the new free block*/
    bp += DSIZE;
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(PREP(bp), 0);
    PUT(SUSP(bp), 0);
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    /*printf("extending\n");
    printf("%p %p %p %lu %lu\n",  HDRP(bp), bp, FTRP(bp), GET_SIZE(HDRP(bp)), GET_ALLOC(HDRP(bp)));
    fflush(stdout);*/
    /* Coalesce if the previous block was free */
    if (free_list_hd == NULL) {
        free_list_hd = bp;
        free_list_tail = bp;
        printf("initiating free_list_hd: %p\n", free_list_hd = bp);
        fflush(stdout);
    }
    /*else if (FTRP(free_list_tail)+DSIZE==HDRP(bp)) {
        coalesce_left(bp);
        printf("coalescing with the last block that happens to be free\n");
        printf("%p\n",FTRP(free_list_tail)+WSIZE);
        fflush(stdout);
    }
    else {
        printf("moving tails: %p\n", free_list_tail);
        printf("size of the only block: %lu\n", GET_SIZE(HDRP(free_list_tail)));
        fflush(stdout);
        PUT(SUSP(free_list_tail),bp);
        PUT(PREP(bp),free_list_tail);
        PUT(SUSP(bp),0);
        free_list_tail = bp;
        printf("new tails: %p\n", free_list_tail);
        fflush(stdout);
    }*/
    else{
        mm_free(bp);
    }
    printf("extend heap after\n");
    fflush(stdout);
    mm_check();
    return 0;
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
    for (bp = free_list_hd; bp!=0 ; bp = GET(SUSP(bp))) {
        void *hdr_addr = HDRP(bp);
        size_t bsize = GET_SIZE(HDRP(bp));
        printf("find fit %p %lu\n",bp,bsize);
        fflush(stdout);
        //only worth splitting if bsize is great than asize more than memory needed for header and footer
        if (bsize > asize + 2*DSIZE) {
            //Split
            size_t csize = bsize - asize ;
            //second-part unallocated and inherit original predecessor and successor
            void *new_bp = hdr_addr+asize+DSIZE;
            PUT(HDRP(new_bp), PACK(csize, 0));
            PUT(PREP(new_bp),GET(PREP(bp)));
            if(GET(PREP(bp)) != 0) {
                PUT(SUSP(GET(PREP(bp))), new_bp);
            }
            else {
                PUT(PREP(new_bp),0);
                free_list_hd = new_bp;
            }
            if(GET(SUSP(bp))!=0) {
                PUT(PREP(GET(SUSP(bp))), new_bp);
            }
            else {
                PUT(SUSP(new_bp),0);
                free_list_tail = new_bp;
            }
            PUT(FTRP(new_bp), PACK(csize, 0));
            //first-half allocated
            PUT(hdr_addr, PACK(asize, 1));
            PUT(hdr_addr+asize-WSIZE, PACK(asize, 1));
            return bp;
        }
        else if (asize == bsize) {
            PUT(hdr_addr, PACK(bsize, 1));
            PUT(FTRP(bp), PACK(bsize, 1));
            if(GET(PREP(bp))!=0 && GET(SUSP(bp))!=0) {
                PUT(SUSP(GET(PREP(bp))),GET(SUSP(bp)));
                PUT(PREP(GET(SUSP(bp))),GET(PREP(bp)));
            }
            else if(GET(PREP(bp))!=0 && GET(SUSP(bp))==0) {
                PUT(SUSP(GET(PREP(bp))),0);
                free_list_tail = GET(PREP(bp));
            }
            else if(GET(PREP(bp))==0 && GET(SUSP(bp))!=0) {
                PUT(PREP(GET(SUSP(bp))),0);
                free_list_hd = GET(SUSP(bp));
            }
            else {
                free_list_hd = NULL;
                free_list_tail = NULL;
            }
            
            return bp;
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
 * find the free block that is immediate to the right of 
   the block that we are about to free
 * then we will call coalesce_free_list on with these two pieces
   for information, the block that we are about to free and the
   free block that is iimmediate to the right of it
 * there are two special cases that cannot be handled by coalesce_free_list;
   thus, will be handled directly in mm_free
 * 1. free_list_hd==NULL or bp itself is NULL
 * 2. want to insert/coalesce to the right of the last free block
 **********************************************************/
void mm_free(void *bp)
{
    printf("mm_free before\n");
    fflush(stdout);
    mm_check();
    if(bp == NULL){
      return;
    }
    /* case 1 */
    if (free_list_hd == NULL) {
        fflush(stdout);
        free_list_hd = bp;
        free_list_tail = bp;
        size_t size = GET_SIZE(HDRP(bp));
        PUT(HDRP(bp), PACK(size,0));
        PUT(PREP(bp), 0);
        PUT(SUSP(bp), 0);
        PUT(FTRP(bp), PACK(size,0));
    }
    else {
        char *cur = free_list_hd;
        while (GET(SUSP(cur)) !=0) {
            //printf("%p\n", GET(SUSP(cur)) );
            //fflush(stdout);
            if (cur > bp) {
                break;
            }
            cur = GET(SUSP(cur));
        }
        if ( GET(SUSP(cur))==0 && bp > cur) {
            /* case 2 */
            if(cur == PREV_BLKP(bp)){
                coalesce_left(bp);
            }
            else {
                //printf("mm_free insert free blk at end: %p\n",bp);
                //fflush(stdout);
                size_t size = GET_SIZE(HDRP(bp));
                PUT(HDRP(bp), PACK(size,0));
                PUT(FTRP(bp), PACK(size,0));
                PUT(SUSP(cur),bp);
                PUT(PREP(bp),cur);
                PUT(SUSP(bp),0);
                free_list_tail = bp;
            }
            
        }
        else {
            coalesce_free_list(bp,cur);
        }
    }
    /* old stuff
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);*/
    printf("mm_free after\n");
    fflush(stdout);
    mm_check();
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
    printf("mm_malloc before %lu\n", size);
    fflush(stdout);
    mm_check();
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
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    /*printf("there\n");
    fflush(stdout);*/
    extendsize = MAX(asize, CHUNKSIZE);
    if (extend_heap(extendsize/WSIZE) == 1){
        return NULL;
    }
    bp = find_fit(asize);
    printf("mm_malloc after %lu\n", size);
    fflush(stdout);
    mm_check();
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    printf("realloc\n");
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
    if (free_list_hd==NULL) {
        return 0;
    }
    printf("head: %p\n", free_list_hd);
    void *cur = free_list_hd;
    while (GET(SUSP(cur))!=0) {
        printf("prev free: %p bp:%p nxt free: %p size:%lu\n",GET(PREP(cur)),cur,GET(SUSP(cur)), GET_SIZE(HDRP(cur)));
        cur = GET(SUSP(cur));
    }
    printf("prev free: %p bp:%p nxt free: %p size:%lu\n",GET(PREP(cur)),cur,GET(SUSP(cur)), GET_SIZE(HDRP(cur)));
    printf("tail: %p\n", free_list_tail);
    fflush(stdout);
  return 0;
}

int mm_check_brk(void *new_first_byte) {
    void *old_brk = heap_listp;
    while(! (GET_SIZE(FTRP(old_brk)+WSIZE)==0 && GET_ALLOC(FTRP(old_brk+WSIZE)==1))) {
        old_brk = NEXT_BLKP(old_brk);
    }
    return !(old_brk == new_first_byte-WSIZE);
}
