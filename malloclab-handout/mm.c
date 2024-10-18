/*
 * Implement explicit free lists
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

#define MIN_BLK_SIZE 16

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

/* Move to the prev free block */
#define PREV_FREE_BLKP(ptr) ((char *)(*(uint32_t *)(ptr)))

/* Move to the next free block */
#define NEXT_FREE_BLKP(ptr) ((char *)(*((uint32_t *)(ptr) + 1)))

/* Set the previous free block addr in the payload */
#define SET_PREV_FREE_BLKP(ptr, prev_ptr) (PUT(ptr, (uint32_t) (prev_ptr)))

/* Set the next free block addr in the payload */
#define SET_NEXT_FREE_BLKP(ptr, next_ptr) (PUT(((char *)(ptr) + WSIZE), (uint32_t) (next_ptr)))

/* basic pointer for free list */
static void *free_bp;

/* first-match policy */
static void *find_first_fit(uint32_t bytes);

/* find-best policy */
static void *find_best_fit(uint32_t bytes);

static void split(void *ptr, uint32_t bytes);

static void move_blk_out_of_free_list(void *ptr);

static void insert_free_list(void *ptr);

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

    /* Initially, no free block */
    free_bp = NULL;
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

    /* include header and footer, plus 2 pointers space */
    size += DSIZE * 2;

    /* align the size by 8 */
    size = ALIGN(size);

    /* Search the suitable block */
    // char *ptr = (char *)find_first_fit(size);
    char *ptr = (char *)find_best_fit(size);

    /* increase heap if no suitable block */
    if (ptr == NULL) {
        ptr = (char *)incr_heap(size);
    }

    /* move the block pointed by ptr out of free list */
    move_blk_out_of_free_list(ptr);

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
    void* new_ptr = coalesce(ptr);
    insert_free_list(new_ptr);

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

    if (ptr == NULL)
        return mm_malloc(size);

    void *newptr;
    size_t copySize;

    copySize = GETSIZE(HDRP(ptr)) - DSIZE;
    /* if size is smaller than copySize, no need to malloc */
    if (size < copySize) {
        /* keep header and footer */
        if (size + DSIZE < MIN_BLK_SIZE) 
            size = MIN_BLK_SIZE;
        else 
            size = ALIGN(size + DSIZE);
        split(ptr, size);
        newptr = ptr;

        // printf("%s: %d\n", "rellocate smaller size", size);
        // printBlock();
    } else if (size == copySize) {
        newptr = ptr;
    } else {
        /* try to use the blocks next to the curr block if they are free */
        char *nxt_blk = NEXT_BLKP(ptr);
        char *last_blk = PREV_BLKP(ptr);
        uint32_t nxt_blk_alloc = GETALLOC(HDRP(nxt_blk));
        uint32_t prev_blk_alloc = GETALLOC(HDRP(last_blk));
        uint32_t nxt_blk_sz = nxt_blk_alloc? 0: GETSIZE(HDRP(nxt_blk));
        uint32_t prev_blk_sz = prev_blk_alloc? 0: GETSIZE(HDRP(last_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        uint32_t total_sz = prev_blk_sz + curr_blk_sz + nxt_blk_sz;
        uint32_t new_assign_size = ALIGN(size + DSIZE);

        if (total_sz >= new_assign_size) {
            /* set up new pointer */
            if (!prev_blk_alloc) 
                newptr = last_blk;
            else
                newptr = ptr;
                
            /* set up header */
            if (!prev_blk_alloc) 
                PUT(HDRP(last_blk), total_sz);
            else
                PUT(HDRP(ptr), total_sz);

            /* set up footer */
            if (!nxt_blk_alloc)
                PUT(FTRP(nxt_blk),total_sz);
            else
                PUT(FTRP(ptr), total_sz);

            /* move the blocks out of free list*/
            if (!prev_blk_alloc)
                move_blk_out_of_free_list(last_blk);
            if (!nxt_blk_alloc)
                move_blk_out_of_free_list(nxt_blk);

            /* copy the content of current block to the beginning of the prev block */
            if (newptr == last_blk) {
                char *dst_ptr = last_blk;
                char *src_ptr = ptr;
                uint32_t copy_step = prev_blk_sz >= copySize? copySize: prev_blk_sz;
                uint32_t nsize = 0;
                while (nsize + copy_step <= copySize) {
                    memcpy(dst_ptr, src_ptr, copy_step);
                    // printf("blk_sz: %d, dst: %p, src: %p, end_dst: %p\n", prev_blk_sz, dst_ptr, src_ptr, dst_ptr + copy_step);
                    // fflush(stdout);
                    dst_ptr += copy_step;
                    src_ptr += copy_step;
                    nsize += copy_step;
                }
                if (nsize < copySize) {
                    // printf("dst: %p, src: %p, end_dst: %p, footer: %p\n", dst_ptr, src_ptr, dst_ptr + copySize - nsize, FTRP(last_blk));
                    // fflush(stdout);
                    memcpy(dst_ptr, src_ptr, copySize - nsize);
                }
            }
            /* set up alloc and the optional further split */
            split(newptr, new_assign_size);
            
        } else {
            newptr = mm_malloc(size);
            memcpy(newptr, ptr, copySize);
            mm_free(ptr);
        }

    }
    
    return newptr;
}


static void *find_first_fit(uint32_t bytes) {

    char *ptr = (char *)free_bp;

    /* if block is allocated or the size of the block is smaller than required, keep moving */
    while ((ptr != NULL) && (GETSIZE(HDRP(ptr)) < bytes)) {
        ptr = PREV_FREE_BLKP(ptr);
    }
    
    return ptr;
}

static void *find_best_fit(uint32_t bytes) {
    char *ptr = (char *)free_bp;
    char *best_fit = NULL;
    uint32_t best_size = UINT32_MAX;
    uint32_t blk_size;
    /* if block is allocated or the size of the block is smaller than required, keep moving */
    while (ptr != NULL) {
        blk_size = GETSIZE(HDRP(ptr));
        if (blk_size >= bytes) {
            if (best_fit == NULL || blk_size < best_size) {
                best_size = blk_size;
                best_fit = ptr;
            }
        }
        ptr = PREV_FREE_BLKP(ptr);
    }
    
    /* no available block */
    return best_fit;
}

static void split(void *ptr, uint32_t bytes) {
    uint32_t blk_size = GETSIZE(HDRP(ptr));
    uint32_t left_size;
    /* if we have more than 8 bytes(header + footer) left, split the block */
    if ((blk_size > bytes) && ((left_size = blk_size - bytes) >= MIN_BLK_SIZE)) {
        PUT(HDRP(ptr), PACK(bytes, 1));
        PUT(FTRP(ptr), PACK(bytes, 1));
        char *nxt_blk = NEXT_BLKP(ptr);
        // printf("\n%p, %p, %d, %d, %d\n", ptr, nxt_blk, left_size, blk_size, bytes);

        /* insert the remaining block back to free list*/
        PUT(HDRP(nxt_blk), left_size);
        PUT(FTRP(nxt_blk), left_size);
        /* normal malloc doesn't need the coalesce since every time calling free will call coalesce. */
        /* But for realloc and the realloc size is smaller than original, then the split block might coalesce with the next block */
        coalesce(nxt_blk);
        insert_free_list(nxt_blk);
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

    /* insert free block at the head of free list */
    insert_free_list(bp);

    return bp;
}

static void move_blk_out_of_free_list(void *ptr) {
    char *prev_free_blk_ptr = PREV_FREE_BLKP(ptr);
    char *nxt_free_blk_ptr = NEXT_FREE_BLKP(ptr);
    if (nxt_free_blk_ptr != NULL)
        SET_PREV_FREE_BLKP(nxt_free_blk_ptr, prev_free_blk_ptr);
    if (prev_free_blk_ptr != NULL)
        SET_NEXT_FREE_BLKP(prev_free_blk_ptr, nxt_free_blk_ptr);
    /* if the next free block ptr is NULL, meaning the block is the head in the free list, set free_ptr to the prev block */
    if (nxt_free_blk_ptr == NULL)
        free_bp = prev_free_blk_ptr;
}

static void insert_free_list(void *ptr) {
    SET_PREV_FREE_BLKP(ptr, free_bp);
    SET_NEXT_FREE_BLKP(ptr, 0);
    if (free_bp != NULL)
        SET_NEXT_FREE_BLKP(free_bp, ptr);
    free_bp = ptr;
}

static void *coalesce(void *ptr) {
    char *last_blk = PREV_BLKP(ptr);
    char *nxt_blk = NEXT_BLKP(ptr);
    uint32_t last_blk_alloc = GETALLOC(HDRP(last_blk));
    uint32_t nxt_blk_alloc = GETALLOC(HDRP(nxt_blk));
   
    if (last_blk_alloc && nxt_blk_alloc) 
        return ptr;
    else if (last_blk_alloc && (!nxt_blk_alloc)) {
        uint32_t nxt_blk_sz = GETSIZE(HDRP(nxt_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        /* update the header and footer */
        PUT(HDRP(ptr), curr_blk_sz + nxt_blk_sz);
        PUT(FTRP(nxt_blk), curr_blk_sz + nxt_blk_sz);
        /* move the next block out of free list */
        move_blk_out_of_free_list(nxt_blk);
        return ptr;
    } else if ((!last_blk_alloc) && nxt_blk_alloc) {
        uint32_t prev_blk_sz = GETSIZE(HDRP(last_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        /* update the header and footer */
        PUT(HDRP(last_blk), curr_blk_sz + prev_blk_sz);
        PUT(FTRP(ptr), curr_blk_sz + prev_blk_sz);
        /* move the prev block out of free list */
        move_blk_out_of_free_list(last_blk);
        return last_blk;
    } else {
        uint32_t nxt_blk_sz = GETSIZE(HDRP(nxt_blk));
        uint32_t prev_blk_sz = GETSIZE(HDRP(last_blk));
        uint32_t curr_blk_sz = GETSIZE(HDRP(ptr));
        /* update the header and footer */
        PUT(HDRP(last_blk), curr_blk_sz + prev_blk_sz + nxt_blk_sz);
        PUT(FTRP(nxt_blk), curr_blk_sz + prev_blk_sz + nxt_blk_sz);
        /* move the two blocks out of free list */
        move_blk_out_of_free_list(last_blk);
        move_blk_out_of_free_list(nxt_blk);
        return last_blk;
    }
    
}


static void printBlock() {
    char *ptr = (char *)mem_heap_lo();
    ptr += DSIZE;
    char *ptr_end = (char *)mem_heap_hi();
    while (ptr <= ptr_end) {
        printf("[%p, %d, %d]=>", ptr, GETSIZE(HDRP(ptr)), GETALLOC(HDRP(ptr)));
        ptr = NEXT_BLKP(ptr);
    }
    printf("%s\n", "");
    fflush(stdout);
}