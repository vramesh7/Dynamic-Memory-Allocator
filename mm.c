/*
 * Malloc lab CSAPP => Explicit list-LIFO list
 *
 * Block structure: 
 * header				4 bytes
 * Footer				4 bytes
 * Atleast 2 payload sets		8 bytes
 * Therefore, Minimum block size	16 bytes
 * Allocated block format:
 * [HEADER:---PAYLOAD---:FOOTER]	=> Block format
   0      3             19     23 	=> bytes
 *
 * Free block format:
 * [HEADER:Prev:Next---:FOOTER]		=> Block format
   0      3    7   11   19    23    	=> bytes
 * Every Free block has pointers for next and free blocks that are placed in an explicit doubly linked list of free blocks 
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/***********HELPER FUNCTIONS********************/
static void *coalesce(void *);
static void *extend_heap(size_t);
static void *find_fit(size_t asize);
static void place(void *, size_t);
/***************PROTOTYPES********************/


/******LINKED LIST FUNCTIONS*****************/
static void insertblock(void *bp); 
static void deleteblock(void *bp);
/***************PROTOTYPES*******************/

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





/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
//#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


static char *heap_listp = 0;  		/* Pointer to first block */  
static char *freelist_head = 0;		/* Pointer to first free block */
static char *freelist_tail = 0;		/* Pointer to last free block in the list */

/* 
 * Function Name:	mm_init
 * Argument:		None
 * Return Type: 	void
 * Description:		Initialize empty heap and maintain byte ordering and coalescing boundry by creating prologue and epilogue blocks
			 
 */

int mm_init(void) 
{
/* Create the initial empty heap */
	if ((heap_listp = mem_sbrk(2*MIN_BLOCK_SIZE)) == NULL) 
		return -1;

	PUT(heap_listp, 0);							/* Alignment padding */ 
	PUT(heap_listp + WSIZE, PACK(MIN_BLOCK_SIZE, 1));			/* Prologue header */
	PUT(heap_listp + DSIZE, 0);						/* Previous pointer */
	PUT(heap_listp + DSIZE+WSIZE, 0);					/* Next pointer */ 
	
	
	PUT(heap_listp + MIN_BLOCK_SIZE, PACK(MIN_BLOCK_SIZE, 1));		/* Prologue footer */ 
	PUT(heap_listp+WSIZE + MIN_BLOCK_SIZE, PACK(0, 1));			/* Epilogue header */ 

/* Initialize linked list head and tail to point to start of heap */	
	freelist_head = heap_listp + DSIZE;					
	freelist_tail = freelist_head; 
/* Extend the empty heap with a free block of CHUNKSIZE bytes */
	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
		return -1;
	return 0;
}


/* 
 * Function Name:	mm_malloc
 * Argument:		Memory block size requested in bytes
 * Return Type: 	Pointer to block of memory
 * Description:		Allocate the requested number of bytes on the heap from freelist and return the header pointer. 
			If free list can't satisfy the request, extend heap size by appropriate number of bytes. 
 */

void *mm_malloc(size_t size) 
{
	size_t asize;						/* Adjusted block size */      
	size_t extendsize;					/* Amount to extend heap if no fit */ 
	char *bp;

	
	if (size <= 0)						/* return if illegal malloc call */
		return NULL;

/* Adjust block size to include overhead and alignment reqs. i.e. enforcing minimum block size requirement*/	
	asize = MAX(ALIGN(size) + DSIZE, MIN_BLOCK_SIZE);

/* Search the free list for a fit */	
	if ((bp = find_fit(asize))) 				/*Check if a block can satisfy the requested memory*/
	{
		place(bp, asize);				/*Check block and decide whether to split or not */
		return bp;
	}

/* No fit found. Get more memory and place the block by extending the heap*/
	extendsize = MAX(asize, CHUNKSIZE);
	
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
	{ 
		return NULL;
	}
	place(bp, asize);
	return bp;
} 



/* 
 * Function Name:	mm_free
 * Argument:		Pointer to block of memory to be freed
 * Return Type: 	void
 * Description:		Free the memory block pointed by the block pointer
			 
 */

void mm_free(void *bp)
{
	if(bp == NULL)					/* Return if illegal free call,i.e. null pointer free call */
	{
		return; 
	}	
	size_t size = GET_SIZE(HDRP(bp));		/* size of block to be freed */
/* Update header and footer of block with free allocation status*/
	PUT(HDRP(bp), PACK(size, 0)); 
	PUT(FTRP(bp), PACK(size, 0));
	coalesce(bp); 
}


/* 
 * Function Name:	coalesce
 * Argument:		pointer to block
 * Return Type: 	updated pointer to free block
 * Description:		Check the allocation status of the previous and the next block after freeing a block of memory and coalesce them 				together to form a larger block if applicable.
			 
 */
static void *coalesce(void *bp) 
{
	//size_t prev_alloc;
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && !next_alloc)					/* Previous block is allocated and next block is free */ 
	{			
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		deleteblock(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && next_alloc)				/* Previous block is free and next block is allocated */ 
	{		
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		bp = PREV_BLKP(bp);
		deleteblock(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}

	else if (!prev_alloc && !next_alloc)				/* previous and next blocks are free */ 
	{		
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
				GET_SIZE(HDRP(NEXT_BLKP(bp)));
		deleteblock(PREV_BLKP(bp));
		deleteblock(NEXT_BLKP(bp));
		bp = PREV_BLKP(bp);
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	
	insertblock(bp);
	
	return bp;
}

/* 
 * Function Name:	mm_realloc
 * Argument:		pointer to block
 * Return Type: 	updated pointer to allocated block, size to which it is to be reallocated
 * Description:		Update the size of the already existing allocated malloced block of memory to the size provided in argument
			 
 */

void *mm_realloc(void *ptr, size_t size)
{
	size_t oldsize;
	void *newptr;
	size_t asize = MAX(ALIGN(size) + DSIZE, MIN_BLOCK_SIZE);
	/* If size <= 0 then this is just free, and we return NULL. */
	if(size <= 0) {
		mm_free(ptr);
		return 0;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if(ptr == NULL) {
		return mm_malloc(size);
	}

	/* Get the size of the original block */
	oldsize = GET_SIZE(HDRP(ptr));
	
	/* If the size doesn't need to be changed, return orig pointer */
	if (asize == oldsize)
		return ptr;
	
	/* If the size needs to be decreased, shrink the block and 
	 * return the same pointer */
	if(asize <= oldsize)
	{
		size = asize;

		/* If a new block couldn't fit in the remaining space, 
		 * return the pointer */
		if(oldsize - size <= MIN_BLOCK_SIZE)
			return ptr;
		PUT(HDRP(ptr), PACK(size, 1));
		PUT(FTRP(ptr), PACK(size, 1));
		PUT(HDRP(NEXT_BLKP(ptr)), PACK(oldsize-size, 1));
		mm_free(NEXT_BLKP(ptr));
		return ptr;
	}

	newptr = mm_malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if(!newptr) {
		return 0;
	}

	/* Copy the old data. */
	if(size < oldsize) oldsize = size;
	memcpy(newptr, ptr, oldsize);

	/* Free the old block. */
	mm_free(ptr);

	return newptr;
}

/* 
 * checkheap - We don't check anything right now. 
 */
void mm_checkheap(int verbose)  
{ 
}


/* 
 * Function Name:	extend_heap
 * Argument:		Size by which the heap is to be extended
 * Return Type: 	Pointer to old location of brk
 * Description:		Extend the size by word size in the argument
			 
 */
static void *extend_heap(size_t words) 
{
	char *bp;
	size_t size;

/* Allocate an even number of words to maintain alignment */	
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
	if (size < MIN_BLOCK_SIZE)
		size = MIN_BLOCK_SIZE;
	if ((long)(bp = mem_sbrk(size)) == -1) 
		return NULL;

/* Initialize free block header/footer and the epilogue header */	
	PUT(HDRP(bp), PACK(size, 0));         				/* free block header */
	PUT(FTRP(bp), PACK(size, 0));         				/* free block footer */
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 				/* new epilogue header */

/* Coalesce if the previous block was free */	
	return coalesce(bp);
                                         
}


/* 
 * Function Name:	place
 * Argument:		Block pointer, Size of block
 * Return Type: 	Void
 * Description:		Allocate block in a free block, check if the free block has enough space after allocation to be an independent free 
			block, then split the block to avoid internal fragmentation.
 */

static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));


	if ((csize - asize) >= MIN_BLOCK_SIZE)		/* Difference is large enough to be an independent block, so split the blocks */ 
	{
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		deleteblock(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		coalesce(bp);
	}
	
	else {						/* Donot split the block, small internal fragmentation will happen */
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		deleteblock(bp);
	}
}


/* 
 * Function Name:	find_fit
 * Argument:		Size of block
 * Return Type: 	pointer to block
 * Description:		Search for the first block in the implicit free-list that fits and satisfies the allocation request and return the 				pointer for that block.
 */

static void *find_fit(size_t asize)

{
	void *bp;
/* First fit algorithm, Check for the first block that fits from the start of the heap that satisfies the request */	
	for (bp = freelist_head; GET_ALLOC(HDRP(bp)) == 0; bp = FREE_NEXT(bp)) 
	{
		if (asize <= (size_t)GET_SIZE(HDRP(bp)))
			return bp;
    	}
	return NULL; /* No Fit */

}


/* 
 * Function Name:	insertblock
 * Argument:		pointer to block
 * Return Type: 	void
 * Description:		Insert the new free (or coalesced) block to the head of the free list
 */
static void insertblock(void *bp)
{
	FREE_NEXT(bp) = freelist_head; 
	FREE_PREV(freelist_head) = bp; 
	FREE_PREV(bp) = NULL; 
	freelist_head = bp; 
}

/* 
 * Function Name:	deleteblock
 * Argument:		pointer to block
 * Return Type: 	void
 * Description:		Delete the free block from the free list if the block gets allocated or coalesced with other block to become a larger 				block
 */
static void deleteblock(void *bp)
{
	void *previous = FREE_PREV(bp);
	void *next = FREE_NEXT(bp);
	if (previous) 
		FREE_NEXT(previous) = next;
	else
		freelist_head = next; 
		FREE_PREV(next) = previous;
}
