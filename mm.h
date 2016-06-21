#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

/******************************************/
#define WSIZE 			4			//word size
#define DSIZE 			8			//double word size
#define CHUNKSIZE 		16			//chunksize-initial heap
#define OVERHEAD 		8			//header and footer overhead bits

#define MAX(x,y) 		((x)>(y) ?(x) : (y))	//Find max
#define PACK(size,alloc)  	((size)|(alloc))	//pack allocation status in last bit 

//#define GET(p)			(*(size_t *)(p))	//read value from address p
//#define PUT(p,val)		(*(size_t *)(p) = val)	//write value at address p

#define GET(p)			(*(int *)(p))	//read value from address p
#define PUT(p,val)		(*(int *)(p) = val)	//write value at address p

/* Use get size and get alloc only on header and footer blocks*/
#define GET_SIZE(p)		(GET(p) & ~0x7)		//read size field from address p
#define GET_ALLOC(p)		(GET(p) & 0x1)		//read allocation status field from address p  

#define HDRP(bp)		((void *)(bp) - WSIZE)	//compute address of block header
#define FTRP(bp)		((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)	//compute address of block footer

#define NEXT_BLKP(bp)		((void *)(bp) + GET_SIZE(((void *)(bp) - WSIZE)))	//compute address of next block
#define PREV_BLKP(bp)		((void *)(bp) - GET_SIZE(((void *)(bp) - DSIZE)))	//compute address of previous block




#define FREE_NEXT(bp)(*(void **)(bp + DSIZE))
#define FREE_PREV(bp)(*(void **)(bp))

#define MINIMUM		24			/* Min block size to contain pointers and boundary tags*/
#define MIN_BLOCK_SIZE		24

/*******************************************/

/* 
 *
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

