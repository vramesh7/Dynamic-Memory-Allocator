/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/*******************************/
static void *coalesce(void *);
static void *extend_heap(size_t);
static void *find_fit(size_t);
static void place(void *, size_t);

/********************************/
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Malloc lab",
    /* First member's full name */
    "Vishnu",
    /* First member's email address */
    "vramesh@uncc.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};



#define NEXT_FIT
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))



//char *heap_listp;
static char *heap_listp = 0;  /* Pointer to first block */  
#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	
	/*creating empty heap*/
	if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)		
		return -1;
	
	PUT(heap_listp,0);				//alignment padding
	PUT(heap_listp+WSIZE, PACK(OVERHEAD,1));	//prologue header
	PUT(heap_listp+DSIZE, PACK(OVERHEAD,1));	//prologue footer
	PUT(heap_listp+WSIZE+DSIZE, PACK(0,1));		//epilogue
	heap_listp += DSIZE;				//heap_listp now points to the prologue footer

	#ifdef NEXT_FIT
    	rover = heap_listp;
	#endif

	if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;
    	return 0;
}

/* 
 * mm_malloc - Allocate a block by finding the block in the list.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{

	size_t asize;					//actual size of block
	size_t extendsize;			
	char *bp;					//block pointer
	
	if (heap_listp == 0){
		mm_init();
    	}

	if(size <= 0)					//illegal malloc check
		return NULL;
	
	if(size <= DSIZE)				//Minimum block-size check 
		asize = ALIGN(DSIZE + OVERHEAD);
	else 
		asize = ALIGN(DSIZE * ((size + (OVERHEAD) + (DSIZE-1))/DSIZE));

	if((bp = find_fit(asize)) != NULL){		//find a block using next fit.
		place(bp,asize);
		return bp;
	}
	
	/*If no free block can satisfy the request, only option is to increase the heapsize*/
	
	extendsize = MAX(asize, CHUNKSIZE);		
	if((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);

	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	if(ptr == NULL)				//check for illegal free call
		return;	
	if (heap_listp == 0){			//uninitialized heap
		mm_init();
	}

	size_t size = GET_SIZE(HDRP(ptr));	//find size of the whole block that is being freed
	
	/*Update the header and the footer of the block to show that the block is free*/
	PUT(HDRP(ptr),PACK(size, 0));		
	PUT(FTRP(ptr),PACK(size, 0));
	coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
	mm_free(ptr);
	return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
	return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
	return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}


/*Coalesce adjacent free blocks to one larger block*/
static void *coalesce(void *ptr)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));	//allocation status of previous block
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));	//allocation status of next block 
	size_t size = GET_SIZE(HDRP(ptr));			//Get size of currently freed block

	if(prev_alloc && next_alloc){				//neither of the adjacent blocks are free
		return ptr;
	}

	else if(prev_alloc && (!next_alloc)){			//next block is free
		size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
		PUT(HDRP(ptr), PACK(size,0));
		PUT(FTRP(ptr), PACK(size,0));
	}
	
	else if((!prev_alloc) && next_alloc){			//previous block is free
		size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
		PUT(FTRP(ptr), PACK(size,0));
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size,0));
		ptr = PREV_BLKP(ptr);
	}

	else {							//both the blocks are free
		size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
		PUT(HDRP(PREV_BLKP(ptr)), PACK(size,0));
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(size,0));
		ptr = PREV_BLKP(ptr);
	}
	#ifdef NEXT_FIT
    	/* Make sure the rover isn't pointing into the free block */
    	/* that we just coalesced */
    	if ((rover > (char *)ptr) && (rover < NEXT_BLKP(ptr))) 
		rover = ptr;
	#endif
	return ptr;
}


static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)  
	return NULL;                                        //line:vm:mm:endextend

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

    /* Coalesce if the previous block was free */
    return coalesce(bp);
 
}



static void place(void *bp, size_t asize)
  
{
    size_t csize = GET_SIZE(HDRP(bp));   		//size of 

    if ((csize - asize) >= (2*DSIZE)) { 		//split the blocks, since the difference is large enough to be an independent block
	/* Change from free to allocated */	
	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));			
	bp = NEXT_BLKP(bp);				//Block split, after end of allocated  block
	/* New free block created */
	PUT(HDRP(bp), PACK(csize-asize, 0));
	PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else { 						//Dont split the free block.Will cause low fragmentations
	PUT(HDRP(bp), PACK(csize, 1));
	PUT(FTRP(bp), PACK(csize, 1));
    }
}


/* 
 * find_fit - Find a fit for a block with asize bytes 
 */

static void *find_fit(size_t asize)/* $end mmfirstfit-proto */
{
/* $end mmfirstfit */

#ifdef NEXT_FIT 
    /* Next fit search */
    char *oldrover = rover;

    /* Search from the rover to the end of list */
    for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
	if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    /* search from start of list to old rover */
    for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
	if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	    return rover;

    return NULL;  /* no fit found */
#else 
/* $begin mmfirstfit */

    /* First fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
	    return bp;
	}
    }
    return NULL; /* No fit */
/* $end mmfirstfit */
#endif
}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {
	printf("%p: EOL\n", bp);
	return;
    }

    /*  printf("%p: header: [%p:%c] footer: [%p:%c]\n", bp, 
	hsize, (halloc ? 'a' : 'f'), 
	fsize, (falloc ? 'a' : 'f')); */
}

static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
	printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
	printf("Error: header does not match footer\n");
}

/* 
 * checkheap - Minimal check of the heap for consistency 
 */
void checkheap(int verbose) 
{
    char *bp = heap_listp;

    if (verbose)
	printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
	printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	if (verbose) 
	    printblock(bp);
	checkblock(bp);
    }

    if (verbose)
	printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
	printf("Bad epilogue header\n");
}


