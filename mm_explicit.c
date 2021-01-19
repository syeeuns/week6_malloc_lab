#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
team_t team = {
    /* Team name */
    "3-1",
    /* First member's full name */
    "yeny",
    /* First member's email address */
    "yeny@kakao.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};
// ------------------- Edit Start ------------------- //
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define MINBLOCK    16
#define CHUNKSIZE   (1<<10) /* Extend heap by this amount (bytes) */     
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc)   ((size) | (alloc))
#define ALIGN(size) (((size) + (DSIZE-1)) & ~0x7)
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)    ((char *)(bp) - WSIZE)            
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)     
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREDP(bp)    (*(char **)(bp))
#define SUCCP(bp)    (*(char **)(bp + WSIZE))

static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void byeBlock(void *bp);
void* heap_listp;           /* 힙의 맨 처음 위치를 가리키고 있는 포인터, find_fit을 하는 시작점이 된다*/
void* free_listp;           /* 가용 블록의 시작 위치를 가리키고 있는 포인터 */
#ifdef NEXT_FIT
void* next_fitp;           /* Next_fit 사용 시 탐색 시작 위치를 지정할 포인터 */
#endif 
// CLEAR
int mm_init(void)
{
    /* Create the initial empty heap */
    // 4 워드 크기 만큼을 추가한다, 
    // Alignment padding, Prologue header, Prologue footer, Epilogue header가 들어갈 공간 확보 
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE
    ), PACK(0, 1));        /* Epilogue header */
    free_listp = NULL; // 초기화 

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){ // 가용부분 확장 
        return -1;
    }
    return 0;
}

// CLEAR
static void *extend_heap(size_t words)
{
    char *bp; // 확장하고 난 데이터의 맨 앞 
    size_t asize;

    /* Allocate an even number of words to maintain alignment */
    /* 더블 워드 할당 기준을 지키기 위해서 추가 해야하는 워드가 홀수면
     * 워드 1개를 더 추가해서 더블 워드의 배수로 조정*/
    asize = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if(asize < MINBLOCK)
        asize = MINBLOCK;

    /* mem_sbrk를 통해 break를 뒤로 옮겨 힙 공간을 size만큼 키운다 */
    if ((long)(bp = mem_sbrk(asize)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    /* 확장한 크기는 가용 블록 header와 footer를 추가하여 가용 블록으로 만들어준다 */
    PUT(HDRP(bp), PACK(asize, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(asize, 0));           /* Free block footer */

    /* 크기를 확장하면서 덮어쓰여진 epilogue header는 추가된 공간 뒤에 만들어준다 */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */

    /* Coalesce if the previous block was free */
    /* 힙 영역이 확장하면서 확장 이전 블록이 가용 영역이었다면 합쳐준다 */
    return coalesce(bp);
}

// CLEAR
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    if (size == 0)
        return NULL;

    // asize = MAX(ALIGN(size) + DSIZE, MINBLOCK);

    if (size <= DSIZE) { // 헤더와 푸터를 고려해야 한다
        asize = MINBLOCK;       
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        /* 공간이 있으면 그 위치에 asize 만큼의 공간 할당 후 포인터 반환*/
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    /* 만약 적절한 공간을 찾지 못했다면 힙 추가 기본 단위인 CHUNKSIZE와 필요한 크기인 asize를 확인해서
     * 더 큰 값을 확장할 크기로 정한다 */
    extendsize = MAX(asize, CHUNKSIZE);
    /* extendsize 만큼 extend_heap 으로 힙 공간을 추가하고 포인터를 반환 */
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    /* 반환된 포인터 위치에 asize 만큼의 크기를 할당 */
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize){
    void *bp;
    for (bp = free_listp; bp != NULL; bp = SUCCP(bp)){
        if (asize <= GET_SIZE(HDRP(bp))) { // free list에서 검사하는 거니까 할당여부 확인 X 
            return bp;
        }
    }
    return NULL; 
}

// CLEAR
static void place(void *bp, size_t asize){ // asize는 할당하려는 데이터 크기 
    /* 할당가능한 공간 전체 크기 */
    size_t csize = GET_SIZE(HDRP(bp));

    /* 할당가능한 공간의 전체 크기와 할당하려는 데이터 크기의 차가 16보다 크면
     * 데이터 크기가 최소인 경우 블록 할당이 가능하므로 공간을 분리한다 */
    if ((csize - asize) >= (MINBLOCK)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        byeBlock(bp);
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        coalesce(bp);
    }
    /* 차이가 16보다 작으면 더블 워드 할당을 만족하기 위해 모든 공간을 사용해야 하므로 공간을 모두 채운다 */
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        byeBlock(bp);
    }
}
void mm_free(void *bp)
{
    if(!bp) return;
    /* 반환 요청한 공간의 크기를 확인 */
    size_t size = GET_SIZE(HDRP(bp));
    /* header와 footer에 가용 공간 표시를 한다 */
    PUT(HDRP(bp), PACK(size, 0));           /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* Free block footer */
    /* 가용 공간을 만들어주고 앞 뒤 공간을 확인해서 합칠 수 있으면 합친다 */
    coalesce(bp);
}
static void *coalesce(void *bp)
{
    // 이전 블록 할당 여부, 다음 블록 할당 여부, 현재 블록의 크기 확인 */
    // 루트가 있는 경우는 || PREV_BLKP(bp) == bp 를 추가해줘야함 
    // 우린 루트 없으니까 ㄱㅊ 
    // size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    // 둘다 할당이면 아무것도 안한다 
    if(prev_alloc && next_alloc){                   
    }

    //case 2
    // 다음 블록이 가용 블록이면 다음 블록과 합친다 
    else if (prev_alloc && !next_alloc) {          
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        byeBlock(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));      
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 3
    // 이전 블록이 가용 블록이면 이전 블록과 합친다 
    else if (!prev_alloc && next_alloc) {          
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp); // 이거 위치 조심 
        byeBlock(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));            
    }

    // case 4
    // 이전 블록과 다음 블록이 가용블록이면 두 블록 모두와 합친다 
    else {                                        
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        byeBlock(PREV_BLKP(bp));
        byeBlock(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp); // 이거 위치 조심 
        PUT(HDRP(bp), PACK(size, 0));               
        PUT(FTRP(bp), PACK(size, 0));          
    }

    // 우리는 루트가 없으니까 free_listp가 가리키는 애를 갈아끼우기로 했음
    // 가리키고 있는 애가 있을 때와 없을 때로 분기 
    if (free_listp == NULL){ // 가리키고 있는 애 없을 때는 맨처음에 걍 넣음 
        free_listp = bp;
        PREDP(bp) = NULL;
        SUCCP(bp) = NULL;
    }
    else{ // 가리키고 있는 애 있을 때는 연결 상태 바꿔줘야지, 이거 순서 매우 중요함 ! (free listp 덮어쓰지 않도록)
        SUCCP(bp) = free_listp;
        PREDP(free_listp) = bp;
        PREDP(bp) = NULL;
        free_listp = bp;
    }
    // 위의 if, else문에서 중복되는 부분 밖으로 꺼내도 됨 
    // 지금은 이해 잘하기 위해 다 씀 
    return bp;
}

// CLEAR
void *mm_realloc(void *ptr, size_t size)
{
    /* 재할당을 요청한 크기가 0이면 공간을 비워달라는 의미이므로 그냥 free하고 종료한다 */
    if (size == 0){
        mm_free(ptr);
        return 0;
    }
    /* 값이 NULL 인 포인터로 요청을 한다면 melloc과 동일한 동작을 한다 */
    if (ptr == NULL){
        return mm_malloc(size);
    }
    /* 새로운 공간을 할당 받아서 그 공간의 포인터를 반환받는다 */
    void *newptr = mm_malloc(size);
    /* 반환 받은 포인터가 NULL 이면 힙 전체 범위에서
     * 할당 가능한 공간이 없다는 이야기이므로 0을 반환한다 */
    if (newptr == NULL){
        return 0;
    }
    /* 할당 가능한 공간이 있으면
     * 현재 공간의 크기를 확인하고*/
    size_t oldsize = GET_SIZE(HDRP(ptr));
    /* 현재 데이터 크기가 옮겨갈 공간의 크기보다 크다면
     * 옮길 수 있는 크기로 조정한다 */
    if (size < oldsize){
        oldsize = size;
    }
    /* 새로운 위치에 이전 위치에 있던 oldsize 만큼 덮어쓴다 */
    memcpy(newptr, ptr, oldsize);
    /* 현재 공간은 반환하고 새로운 위치 포인터를 반환한다 */
    mm_free(ptr);
    return newptr;
}

static void byeBlock(void *bp){
    if(PREDP(bp) == NULL){
        free_listp = SUCCP(bp);
    }
    else{
        SUCCP(PREDP(bp)) = SUCCP(bp);
    }
    if(SUCCP(bp) != NULL)
        PREDP(SUCCP(bp)) = PREDP(bp);
}