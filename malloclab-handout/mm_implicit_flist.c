/*
 * Implement implicit free lists
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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* the maximum heap space is 20M which is more than 24bits, 
 * so at least need 4 bytes to hold the block size and satisfy the allignment.
 */
#define WSIZE 4

#define DSIZE 8

#define PACK(size, alloc) ((size) | (alloc))

#define PUT(ptr, val) (*(uint32_t *)(ptr) = val)

#define GET(ptr) (*(uint32_t *)(ptr))

/* When ptr points to header or footer */
#define GETSIZE(ptr) (GET(ptr) & ~0x7)

#define GETALLOC(ptr) (GET(ptr) & 0x1)

/* When ptr points the beginning of the content block, this MACRO finds the pointer of the header */
#define HDRP(ptr) ((char *)(ptr) - WSIZE)

/* When ptr points the beginning of the content block, this MACRO finds the pointer of the footer */
#define FTRP(ptr) (HDRP(ptr) + GETSIZE(HDRP(ptr)) - WSIZE)

#define SETALLOC(ptr) (PUT(ptr, PACK(GETSIZE(ptr), 1)))

#define FREEALLOC(ptr) (PUT(ptr, GETSIZE(ptr)))

/* Move to the beginning of the next content block */
#define NEXT_BLKP(ptr) (FTRP(ptr) + DSIZE)

/* Move to the beginning of the last content block */
#define PREV_BLKP(ptr) ((char *)(ptr) - GETSIZE(HDRP(ptr) - WSIZE))

static void *bp;

static void *get_bp();

/* first-match policy */
static void *find_first_fit(uint32_t bytes);

/* find-best policy */
static void *find_best_fit(uint32_t bytes);

static void split(void *ptr, uint32_t bytes);

static void *coalesce(void *ptr);

static void *incr_heap(uint32_t bytes);

/* helper function to print the block list */
static void printBlock();


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void *ptr = mem_sbrk(16);
    if (ptr == NULL)
        return -1;
    /* first 4 bytes are skipped for allignment */
    /* put header */
    PUT(ptr + WSIZE, PACK(DSIZE, 1));
    /* put footer */
    PUT(ptr + DSIZE, PACK(DSIZE, 1));
    /* put dummy end header */
    PUT(ptr + WSIZE + DSIZE, PACK(0, 1));
    /* Initialize global start bp */
    bp = get_bp();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by traveling through all the blocks.
 *     
 */
void *mm_malloc(size_t size)
{

    if (size == 0)
        return NULL;

    /* include header and footer */
    size += DSIZE;

    /* align the size by 8 */
    size = ALIGN(size);

    /* Search the suitable block */
    char *ptr = (char *)find_first_fit(size);
    // char *ptr = (char *)find_best_fit(size);

    /* increase heap if no suitable block */
    if (ptr == NULL) {
        ptr = (char *)incr_heap(size);
    }

    /* see if we can split the block. Set up the alloc bits here as well no matter if we can split or not.*/
    if (ptr != NULL)
        split(ptr, size);

    // printf("%s %d\n", "Finish alloc", size);
    // printBlock();
    
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    FREEALLOC(HDRP(ptr));
    FREEALLOC(FTRP(ptr));
    coalesce(ptr);

    // printf("%s\n", "finish free");
    // printBlock();
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
        return ptr;
    }

    void *newptr;
    size_t copySize;
    
    if (ptr != NULL) {
        copySize = GETSIZE(HDRP(ptr)) - DSIZE;
        /* if size is smaller than copySize, no need to malloc */
        if (size < copySize) {
            /* keep header and footer */
            split(ptr, ALIGN(size + DSIZE));
            newptr = ptr;

            // printf("%s: %d\n", "rellocate smaller size", size);
            // printBlock();
        } else if (size == copySize) {
            newptr = ptr;
        } else {
            newptr = mm_malloc(size);
            memcpy(newptr, ptr, copySize);
            mm_free(ptr);
        }
    } else {
        newptr = mm_malloc(size);
    }
    
    return newptr;
}


static void *get_bp() {
    char *ptr = (char *)mem_heap_lo();
    ptr += DSIZE;   
    return ptr;
}

static void *find_first_fit(uint32_t bytes) {

    char *ptr = (char *)bp;
    char *ptr_end = (char *)mem_heap_hi();
    /* if block is allocated or the size of the block is smaller than required, keep moving */
    while ((ptr <= ptr_end) && (GETALLOC(HDRP(ptr)) || (GETSIZE(HDRP(ptr)) < bytes))) {
        ptr = NEXT_BLKP(ptr);
    }
    
    /* no available block */
    if (ptr > ptr_end)
        return NULL;
    else 
        return ptr;
}

static void *find_best_fit(uint32_t bytes) {
    char *ptr = (char *)bp;
    char *ptr_end = (char *)mem_heap_hi();
    char *best_fit = NULL;
    uint32_t best_size = UINT32_MAX;
    uint32_t blk_size;
    /* if block is allocated or the size of the block is smaller than required, keep moving */
    while ((ptr <= ptr_end)) {
        blk_size = GETSIZE(HDRP(ptr));
        if (!GETALLOC(HDRP(ptr)) && (blk_size >= bytes)) {
            if (best_fit == NULL || blk_size < best_size) {
                best_size = blk_size;
                best_fit = ptr;
            }
        }
        ptr = NEXT_BLKP(ptr);
    }
    
    /* no available block */
    return best_fit;
}

static void split(void *ptr, uint32_t bytes) {
    uint32_t blk_size = GETSIZE(HDRP(ptr));
    uint32_t left_size;
    /* if we have more than 8 bytes(header + footer) left, split the block */
    if ((blk_size > bytes) && ((left_size = blk_size - bytes) > DSIZE)) {
        PUT(HDRP(ptr), PACK(bytes, 1));
        PUT(FTRP(ptr), PACK(bytes, 1));
        char *nxt_blk = NEXT_BLKP(ptr);
        // printf("\n%p, %p, %d, %d, %d\n", ptr, nxt_blk, left_size, blk_size, bytes);
        PUT(HDRP(nxt_blk), left_size);
        PUT(FTRP(nxt_blk), left_size);
        /* normal malloc doesn't need the coalesce since every time calling free will call coalesce. */
        /* But for realloc and the realloc size is smaller than original, then the split block might coalesce with the next block */
        coalesce(nxt_blk);
    } else {
        SETALLOC(HDRP(ptr));
        SETALLOC(FTRP(ptr));
    }
        
}

static void *incr_heap(uint32_t bytes) {
    // void *bp = mem_sbrk(mem_pagesize());

    /* if the prev block is not allocated, only assign bytes - (size of prev block) */
    char *heap_top_footer = (char *)mem_heap_hi() + 1 - DSIZE;
  
    if (!GETALLOC(heap_top_footer))
        bytes -= GETSIZE(heap_top_footer);

        
    char *bp = (char *) mem_sbrk(bytes);
    if (bp == (void *)-1) {
        return NULL;
    }
    /* set up header and footer */
    PUT(bp - WSIZE, bytes);
    PUT(bp + bytes - DSIZE, bytes);
    /* set up the ending dummy header */
    PUT(bp + bytes - WSIZE, PACK(0, 1));
    bp = coalesce(bp);
    return bp;
}


static void *coalesce(void *ptr) {
    char *last_blk = PREV_BLKP(ptr);
    char *nxt_blk = NEXT_BLKP(ptr);
    uint32_t last_blk_alloc = GETALLOC(HDRP(last_blk));
    uint32_t nxt_blk_alloc = GETALLOC(HDRP(nxt_blk));
    // printf("In coalesce, %p, %p, %p\n", last_blk, ptr, nxt_blk);
    if (last_blk_alloc && nxt_blk_alloc) 
        return ptr;
    else if (last_blk_alloc && (!nxt_blk_alloc)) {
        uint32_t nxt_blk_sz = GETSIZE(HDRP(nxt_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        PUT(HDRP(ptr), curr_blk_sz + nxt_blk_sz);
        PUT(FTRP(nxt_blk), curr_blk_sz + nxt_blk_sz);
        return ptr;
    } else if ((!last_blk_alloc) && nxt_blk_alloc) {
        uint32_t prev_blk_sz = GETSIZE(HDRP(last_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        PUT(HDRP(last_blk), curr_blk_sz + prev_blk_sz);
        PUT(FTRP(ptr), curr_blk_sz + prev_blk_sz);
        return last_blk;
    } else {
        uint32_t nxt_blk_sz = GETSIZE(HDRP(nxt_blk));
        uint32_t prev_blk_sz = GETSIZE(HDRP(last_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        PUT(HDRP(last_blk), curr_blk_sz + prev_blk_sz + nxt_blk_sz);
        PUT(FTRP(nxt_blk), curr_blk_sz + prev_blk_sz + nxt_blk_sz);
        return last_blk;
    }
    
}


static void printBlock() {
    char *ptr = (char *)bp;
    char *ptr_end = (char *)mem_heap_hi();
    while (ptr <= ptr_end) {
        printf("[%p, %d, %d]=>", ptr, GETSIZE(HDRP(ptr)), GETALLOC(HDRP(ptr)));
        ptr = NEXT_BLKP(ptr);
    }
    printf("%s\n", "");
    fflush(stdout);
}