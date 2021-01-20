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
    "Yeny",
    /* First member's email address */
    "yeny@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// /* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

// // /* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT - 1) & ~0x7))

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

# define GET(p) (*(unsigned int*)(p))
# define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// define 함수 선언할 때랑, 사용할 때는 괄호 하나씩으로 더 감싸줘야함. 그래서 GET_SIZE 뒤가 저 모양
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

static char *mem_heap;
static char *mem_brk;
static char *mem_max_addr;
static char *heap_listp;
static char *next_ptr;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);


/* 
 * mm_init - initialize the malloc package.
 */
// 할당기 초기화함. 성공이면 0, 실패면 -1 리턴
int mm_init(void)
{
    // 메모리 시스템에서 4워드를 가져와서 빈 가용리스트를 만들 수 있도록 초기화 
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp, 0); // 미사용 패딩워드 넣기 
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록(헤더). 헤더와 푸더로만 구성된 8바이트 할당 블록
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // 프롤로그 블록(푸터). 헤더와 푸더로만 구성된 8바이트 할당 블록
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // 에필로그 블록(헤더). 헤더로만 구성된 크기가 0인 할당 블록
    heap_listp += (2*WSIZE); // 프롤로그 블록 다음을 바라보게 함 

    next_ptr = heap_listp; // next_fit 이용할 경우, next_ptr 초기화 
    // 힙을 CHUNKSIZE 바이트로 확장하고 초기 가용 블록 생성
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;

}

// 새 가용 블록으로 힙을 확장하는 함수 
// 힙 초기화 시, malloc이 적당한 fit을 찾지 못했을 때 정렬을 유지하기 위해 호출 
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    // 요청 크기를 인접 2워드의 배수로 반올림 (홀수면 하나 큰거, 짝수면 그대로)
    // 그 후 sbrk 이용하여 메모리 시스템으로부터 추가적인 힙 공간 요청 
    size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE); // 있으나 마나 ㅇㅇ 
    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    // 가용 블록의 헤더, 풋터, 에필로그 헤더 초기화
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    // 이전 힙이 가용 블록으로 끝났다면, 두 개의 가용 블록을 합치기 위해 coalesce 호출
    return coalesce(bp); // 합쳐진 블록의 포인터 반환 
}

/*
 * mm_free - Freeing a block does nothing.
 */
// 이전에 할당한 블록을 반환한다 
void mm_free(void *bp)
{
    // 잘못된 free 요청일 경우 함수 종료, 이전 프로시저로 return
    if (!bp) return;

    // bp해더에서 block size 읽어옴
    size_t size = GET_SIZE(HDRP(bp));

    // 실제 데이터를 지우는 것 아니라 
    // 헤더, 푸터의 최하위 1비트(할당된 상태)만을 수정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp); // 주위에 빈 블럭 있으면 병합 
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블럭 할당 여부 (0 or 1)
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블럭 할당 여부 (0 or 1)
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블럭 size 

    // case 1
    // 이전, 다음 둘다 할당 -> 블럭 병합 없음
    if (prev_alloc && next_alloc){
        return bp;
    }
    // case 2
    // 이전 블럭 할당, 다음 블럭 가용 -> 다음 블럭과 병합 
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));  // 이 부분이 이해가 안감 PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); 이거 아닌가 
        // 위 주석 답변 : FTRP는 HDRP 이용해서 구하는거니까 위에서 PUT(HDRP 어쩌구) 했으니까 자동으로 FTRP는 ㅇㅇ
    }
    // case 3
    // 이전 블럭 가용, 다음 블럭 할당 -> 이전 블럭과 병합
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // case 4
    // 이전 블럭 가용, 다음 블럭 가용 -> 이전, 다음 블럭과 병합
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    if((next_ptr > (char *)bp) && (next_ptr < NEXT_BLKP(bp))){
        next_ptr = bp;
    }

    return bp;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) // 넘겨받는 인자의 크기 size만큼 메모리 할당 
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0){
        return NULL;
    }

    // 최소 16바이트 크기 블록 구성
    // 8바이트는 정렬 요건 만족 위해
    // 8바이트는 헤더, 풋터 오버헤드 위해
    if (size <= DSIZE){
        asize = 2*DSIZE;
    }
    // 8바이트 넘는 요청 : 오버헤드 바이트 내에 더해주고 인접 8의 배수로 반올림
    else{
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1))/DSIZE); // 8 더하고 (헤더, 푸터), 8의 배수로 올림 
    }

    // 적절한 가용 블록을 가용리스트에서 검색
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize); // 맞는 블록 찾으면 할당기는 place함수로 요청한 블록 배치, 옵션으로 초과부분 분할
        return bp; // 새로 할당한 블록 반환
    }

    // 맞는 블록 찾지 못하면(메모리 부족), 힙을 새로운 가용 블록으로 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize); // 요청한 블록을 이 새 가용 블록에 배치, 옵션으로 초과부분 분할
    return bp; // 새로 할당한 블록 반환
    // int newsize = ALIGN(size + SIZE_T_SIZE); // 넘겨받는 인자 size 정렬
    // void *p = mem_sbrk(newsize); // 위에서 정렬한 값을 이용하여 mem_sbrk 호출, 힙 크기 늘려줌 
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE); // mem_sbrk에서 반환되는 포인터에 SIZE_T_SIZE 만큼 증가시킴 
    //     // 크기 정보를 저장하는 부분을 SIZE_PTR(p)을 이용하여 구하고, 그곳에 size를 저장
    // }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
// 새로운 블록을 할당해서 블록의 크기를 바꾸는 함수
// 이전의 데이터를 복사하고, 이전의 블록을 free시킴 
void *mm_realloc(void *ptr, size_t size) 
{
    // size == 0인 경우, 단지 free 해주는 기능. NULL 반환
    // ptr == NULL인 경우, malloc해주는 기능 
    // 위의 두 가지 경우 아니고, malloc에 의해 반환되는 포인터가 null이 아니면
    // memcpy를 이용하여 이전 블록의 데이터를 복사하고, 이전 블록을 free 시킴 
    if (size == 0){
        mm_free(ptr);
        return 0;
    }

    if (ptr == NULL){
        return mm_malloc(size);
    }

    void *newptr = mm_malloc(size);

    if (newptr == NULL){
        return 0;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize){
        oldsize = size;
    }
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);
    return newptr;
    // void *oldptr = ptr;
    // void *newptr;
    // size_t copySize;

    // newptr = mm_malloc(size);
    // if (newptr == NULL)  
    //     return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    // if (size < copySize)
    //     copySize = size;
    // memcpy(newptr, oldptr, copySize);
    // mm_free(oldptr);
    // return newptr;
}

// 이 함수는 요청한 블록을 가용 블록의 시작 부분에 배치하고, 나머지 부분의 크기가 최소 블록의 크기와 같거나 큰 경우에만 분할
// 다음 블록으로 이동 전, 새롭게 할당한 블록을 배치해야한다 
// csize가 현재 블럭 사이즈고, asize가 들어갈 사이즈
// 애초에 들어오는 asize는 2*DSIZE 이상임 !!!!! 
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){ //최소 16바이트 블록이니까 2DSIZE   
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp); // 다음 블럭 이동
        PUT(HDRP(bp), PACK(csize - asize, 0));  
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);
    }
    else{  // csize == asize 인 경우 그냥 할당하고 끝 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// First fit 이용 fit 찾는 함수 
// first fit은 처음부터 검색하면서 해당 size 블록 찾는 방법
static void *find_fit(size_t asize){
    // // first fit (for)
    // void *bp;
    // for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
    //     if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
    //         return bp; 
    //     }
    // }
    // return NULL;

    // // first-fit (while)
    // void *bp = heap_listp;
    // while (GET_SIZE(HDRP(bp)) != 0){
    //     if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= asize)){
    //         return bp;
    //     }
    //     bp = NEXT_BLKP(bp);
    // }
    // return NULL;

    // // next fit
    char *bp = next_ptr;
    for(next_ptr = next_ptr; GET_SIZE(HDRP(next_ptr)) > 0; next_ptr = NEXT_BLKP(next_ptr)){
        if (!GET_ALLOC(HDRP(next_ptr)) && (asize <= GET_SIZE(HDRP(next_ptr)))){
            return next_ptr;
        }
    }

    for (next_ptr = heap_listp; next_ptr < bp; next_ptr = NEXT_BLKP(next_ptr)){
        if (!GET_ALLOC(HDRP(next_ptr)) && (asize <= GET_SIZE(HDRP(next_ptr)))){
            return next_ptr;
        }
    }
    return NULL;

    // // best fit
    // void *bp = heap_listp;
    // void *best_bp = NULL;

    // while (GET_SIZE(HDRP(bp)) != 0){
    //     if ((!GET_ALLOC(HDRP(bp))) && (GET_SIZE(HDRP(bp)) >= asize)){
    //         if (!best_bp || (GET_SIZE(HDRP(bp))) < (GET_SIZE(HDRP(best_bp)))){
    //             best_bp = bp;
    //         }
    //     }
    //     bp = NEXT_BLKP(bp);
    // }
    // if (best_bp){
    //     return best_bp;
    // }
    // return NULL;

}


