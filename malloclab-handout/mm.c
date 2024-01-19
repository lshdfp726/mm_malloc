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

/** 基本工具宏定义 */
#define WSIZE 4  /** 空闲链表头部/脚部大小 (单位字节) */
#define DSIZE 8  /** 双字大小 (单位字节) */
#define CHUNKSIZE (1 << 12)  /** 默认扩展堆大小 */

#define MAX(x, y)  ((x) > (y) ? (x) : (y))

/** 把 size 和 当前块是否空闲的标志位打包到一起，默认低三位是标识位所以用 |，因为size是最少也是 WSIZE，4个字节即0x100，所以最低位用不到，用来打包标识位刚好合适 */
#define PACK(size, alloc) ((size) | (alloc))


/** 从地址p 读/写 一个字的内容 */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/** 从地址p 读取 size 和 是否空闲的标识位 */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/** 计算块地址bp 的头部和脚部位置 */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/** 计算块地址bp 的前后结点的地址*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) /**获取前一个block 的脚部信息，拿到前一个block大小*/

static void *heap_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //处理序言块指针逻辑,mem_sbrk 内部分配失败返回的就是(void *)-1,所以有这么个奇怪的判断
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        fprintf(stderr,"mem_sbrk failed\n");
        return -1;
    }
        
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /** 序言块头表示已经分配，DSIZE表示序言块大小是 8个字节的双字*/
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /** 序言块尾表示已经分配，DSIZE表示序言块大小是 8个字节的双字*/
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); /** 结尾块不占大小*/
    heap_listp += (2 * WSIZE); /** 指向 序言块中间位置 */
    
    /** 用CHUNKSIZE 字节来扩展空堆 */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        fprintf(stderr,"extend_heap failed\n");
        return -1;
    }
    fprintf(stderr,"return 0\n");
    return 0;
}

/** words 传的是字个数，具体大小需要 words * WSIZE*/
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
    
    /** 对齐，向上取整*/
    size = (words % 2)? (words + 1) *WSIZE: words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); /** head */
    PUT(FTRP(bp), PACK(size, 0)); /** foot */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //扩展之后的块更新尾部块大小为0.
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;  //调整对齐后大小
    size_t extendsize;  //没有合适空闲块，需要通过sbrk 从堆最终扩展的大小
    char *bp; //指向 malloc 返回的空闲块的首地址
    if (size == 0) {
        return NULL;
    }
    //16对齐
    if (size < DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); //(DSIZE)放头和脚  (DSIZE - 1) 表示向上舍入到最接近8的整数倍
    }

    if ((bp = find_fit(asize)) != NULL) 
    {
        place(bp, asize);
        return bp;
    }

    //没有合适大小的空闲块，调用sbrk 
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) 
        return NULL;
    
    place(bp, asize);
    return bp;
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	//     return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

//合并剩余空闲内存块，目前只有两个地方涉及到：extend_heap(扩展对)和free时候会合并
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // fprintf(stderr, "prev_alloc is %d, next_alloc is %d, size is %d\n",prev_alloc, next_alloc,size);
    if (prev_alloc && next_alloc)   //前后都没空，直接返回
        return bp;
    
    if (prev_alloc && !next_alloc) // 前不空，后空，合并后
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        char *temp = NEXT_BLKP(bp);
        char *temp1 = FTRP(temp);
        PUT(temp1, PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) //前空，后不空，合并前
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); //指向前
    }  
    else  //前后都空，合并前后
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

//链表遍历合适大小和空闲标识的内存块。
static void *find_fit(size_t asize) 
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) 
    {
        int flag = GET_ALLOC(HDRP(bp));
        int flag1 = (asize <= GET_SIZE(HDRP(bp)));
        fprintf(stderr, "flag is %d, flag1 is %d\n",flag, flag1);
         //bp 是空闲块，并且asize 符合预期
        if (!flag && flag1) {
            return bp;
        }
    }
    return NULL;
}

//该函数包含了设置即将使用的虚拟内存块和分割大尺寸的内存块
static void place(void *bp, size_t asize) 
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        char *p = NEXT_BLKP(bp); //上面两步给头尾都配置了大小为asize，所以NEXT_BLKP = bp(首地址) + asize(偏移量)
        PUT(HDRP(p), PACK((csize - asize), 0)); //更新剩余的空闲块大小
        PUT(HDRP(p), PACK((csize - asize), 0)); //更新剩余的空闲块大小
    }
    else 
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














