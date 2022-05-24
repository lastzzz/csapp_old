#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include "../header/allocator.h"
#include "../header/algorithm.h"


int heap_init();
uint64_t mem_alloc(uint32_t size);
void mem_free(uint64_t vaddr);

/* ------------------------------------- */
/*  Operating System Implemented         */
/* ------------------------------------- */

void os_syscall_brk()
{
    // an empty function
}

uint32_t extend_heap(uint32_t size)
{
    // round up to page alignment
    size = (uint32_t)round_up((uint64_t)size, 4096);
    if (heap_end_vaddr - heap_start_vaddr + size <= HEAP_MAX_SIZE)
    {
        // do brk system call to request pages for heap
        os_syscall_brk();
        heap_end_vaddr += size;
    }
    else
    {
        return 0;
    }

    uint64_t epilogue = get_epilogue();
    set_allocated(epilogue, ALLOCATED);
    set_blocksize(epilogue, 0);

    return size;
}


/* ------------------------------------- */
/*  Free Block Management Implementation */
/* ------------------------------------- */
#ifdef IMPLICIT_FREE_LIST
int implicit_list_initialize_free_block();
uint64_t implicit_list_search_free_block(uint32_t payload_size, uint32_t *alloc_blocksize);
int implicit_list_insert_free_block(uint64_t free_header);
int implicit_list_delete_free_block(uint64_t free_header);
void implicit_list_check_free_block();
#endif

#ifdef EXPLICIT_FREE_LIST
int explicit_list_initialize_free_block();
uint64_t explicit_list_search_free_block(uint32_t payload_size, uint32_t *alloc_blocksize);
int explicit_list_insert_free_block(uint64_t free_header);
int explicit_list_delete_free_block(uint64_t free_header);
void explicit_list_check_free_block();
#endif

#ifdef REDBLACK_TREE
int redblack_tree_initialize_free_block();
uint64_t redblack_tree_search_free_block(uint32_t payload_size, uint32_t *alloc_blocksize);
int redblack_tree_insert_free_block(uint64_t free_header);
int redblack_tree_delete_free_block(uint64_t free_header);
void redblack_tree_check_free_block();
#endif

static int initialize_free_block()
{
#ifdef IMPLICIT_FREE_LIST
    return implicit_list_initialize_free_block();
#endif

#ifdef EXPLICIT_FREE_LIST
    return explicit_list_initialize_free_block();
#endif

#ifdef REDBLACK_TREE
    return redblack_tree_initialize_free_block();
#endif
}

static uint64_t search_free_block(uint32_t payload_size, uint32_t *alloc_blocksize)
{
#ifdef IMPLICIT_FREE_LIST
    return implicit_list_search_free_block(payload_size, alloc_blocksize);
#endif

#ifdef EXPLICIT_FREE_LIST
    return explicit_list_search_free_block(payload_size, alloc_blocksize);
#endif

#ifdef REDBLACK_TREE
    return redblack_tree_search_free_block(payload_size, alloc_blocksize);
#endif
}

static int insert_free_block(uint64_t free_header)
{
#ifdef IMPLICIT_FREE_LIST
    return implicit_list_insert_free_block(free_header);
#endif

#ifdef EXPLICIT_FREE_LIST
    return explicit_list_insert_free_block(free_header);
#endif

#ifdef REDBLACK_TREE
    return redblack_tree_insert_free_block(free_header);
#endif
}

static int delete_free_block(uint64_t free_header)
{
#ifdef IMPLICIT_FREE_LIST
    return implicit_list_delete_free_block(free_header);
#endif

#ifdef EXPLICIT_FREE_LIST
    return explicit_list_delete_free_block(free_header);
#endif

#ifdef REDBLACK_TREE
    return redblack_tree_delete_free_block(free_header);
#endif
}

static void check_free_block()
{
#ifdef IMPLICIT_FREE_LIST
    implicit_list_check_free_block();
#endif

#ifdef EXPLICIT_FREE_LIST
    explicit_list_check_free_block();
#endif

#ifdef REDBLACK_TREE
    redblack_tree_check_free_block();
#endif
}


/* ------------------------------------- */
/*  Malloc and Free                      */
/* ------------------------------------- */

void check_heap_correctness();

static uint64_t merge_blocks_as_free(uint64_t low, uint64_t high)
{
    assert(low % 8 == 4);
    assert(high % 8 == 4);
    assert(get_firstblock() <= low && low < get_lastblock());
    assert(get_firstblock() < high && high <= get_lastblock());
    assert(get_nextheader(low) == high);
    assert(get_prevheader(high) == low);

    // must merge as free
    uint32_t blocksize = get_blocksize(low) + get_blocksize(high);

    set_blocksize(low, blocksize);
    set_allocated(low, FREE);

    uint64_t footer = get_footer(low);
    set_blocksize(footer, blocksize);
    set_allocated(footer, FREE);

    return low;
}

static uint64_t try_alloc_with_splitting(uint64_t block_vaddr, uint32_t request_blocksize)
{
    if (request_blocksize < 8)
    {
        return NIL;
    }

    uint64_t b = block_vaddr;

    uint32_t b_blocksize = get_blocksize(b);
    uint32_t b_allocated = get_allocated(b);

    if (b_allocated == FREE && b_blocksize >= request_blocksize)
    {
        // allocate this block
        delete_free_block(b);
        uint64_t left_footer = get_footer(b);

        set_allocated(b, ALLOCATED);
        set_blocksize(b, request_blocksize);

        uint64_t b_footer = b + request_blocksize - 4;
        set_allocated(b_footer, ALLOCATED);
        set_blocksize(b_footer, request_blocksize);

        uint32_t left_size = b_blocksize - request_blocksize;
        if (left_size >= 8)
        {
            // split this block `b`
            // b_blocksize - request_blocksize >= 8
            uint64_t left_header = get_nextheader(b);
            set_allocated(left_header, FREE);
            set_blocksize(left_header, b_blocksize - request_blocksize);

            set_allocated(left_footer, FREE);
            set_blocksize(left_footer, b_blocksize - request_blocksize);

            assert(get_footer(left_header) == left_footer);
            insert_free_block(left_header);
        }
        return get_payload(b);
    }

    return NIL;
}

static uint64_t try_extend_heap_to_alloc(uint32_t size)
{
    // get the size to be added
    uint64_t old_last = get_lastblock();

    uint32_t last_allocated = get_allocated(old_last);
    uint32_t last_blocksize = get_blocksize(old_last);

    uint32_t to_request_from_OS = size;
    if (last_allocated == FREE)
    {
        // last block can help the request
        to_request_from_OS -= last_blocksize;
        delete_free_block(old_last);
    }

    uint64_t old_epilogue = get_epilogue();

    uint32_t os_allocated_size = extend_heap(to_request_from_OS);
    if (os_allocated_size != 0)
    {
        assert(os_allocated_size >= 4096);
        assert(os_allocated_size % 4096 == 0);

        uint64_t payload_header = NIL;

        // now last block is different
        // but we check the old last block
        if (last_allocated == ALLOCATED)
        {
            // no merging is needed
            // take place the old epilogue as new last 
            uint64_t new_last = old_epilogue;
            set_allocated(new_last, FREE);
            set_blocksize(new_last, os_allocated_size);

            // set the new footer
            uint64_t new_last_footer = get_footer(new_last);
            set_allocated(new_last_footer, FREE);
            set_blocksize(new_last_footer, os_allocated_size);
            insert_free_block(new_last);

            payload_header = new_last;
        }
        else
        {
            // merging with last_block is needed
            set_allocated(old_last, FREE);
            set_blocksize(old_last, last_blocksize + os_allocated_size);

            uint64_t last_footer = get_footer(old_last);
            set_allocated(last_footer, FREE);
            set_blocksize(last_footer, last_blocksize + os_allocated_size);

            // blocksize is different now
            // consider the balanced tree index on blocksize, it must be reinserted
            insert_free_block(old_last);

            payload_header = old_last;
        }

        // try to allocate
        uint64_t payload_vaddr = try_alloc_with_splitting(payload_header, size);

        if (payload_vaddr != NIL)
        {
#ifdef DEBUG_MALLOC
            check_heap_correctness();
#endif
            return payload_vaddr;
        }
    }

    if (last_allocated == FREE)
    {
        // insert the free last block back
        insert_free_block(old_last);
    }

    // else, no page can be allocated
#ifdef DEBUG_MALLOC
    check_heap_correctness();
    printf("OS cannot allocate physical page for heap!\n");
#endif

    return NIL;
}

int heap_init()
{
    // reset all to 0
    for (int i = 0; i < HEAP_MAX_SIZE / 8; i += 8)
    {
        *(uint64_t *)&heap[i] = 0;
    }

    // heap_start_vaddr is the starting address of the first block
    // the payload of the first block is 8B aligned ([8])
    // so the header address of the first block is [8] - 4 = [4]
    heap_start_vaddr = 0;
    heap_end_vaddr = 4096;

    // set the prologue block
    uint64_t prologue_header = get_prologue();
    set_allocated(prologue_header, ALLOCATED);
    set_blocksize(prologue_header, 8);

    uint64_t prologue_footer = prologue_header + 4;
    set_allocated(prologue_footer, ALLOCATED);
    set_blocksize(prologue_footer, 8);

    // set the epilogue block
    // it's a footer only
    uint64_t epilogue = get_epilogue();
    set_allocated(epilogue, ALLOCATED);
    set_blocksize(epilogue, 0);

    // set the block size & allocated of the only regular block
    uint64_t first_header = get_firstblock();
    set_allocated(first_header, FREE);
    set_blocksize(first_header, 4096 - 4 - 8 - 4);

    uint64_t first_footer = get_footer(first_header);
    set_allocated(first_footer, FREE);
    set_blocksize(first_footer, 4096 - 4 - 8 - 4);

    initialize_free_block();

    return 1;
}

uint64_t mem_alloc(uint32_t size)
{
    assert(0 < size && size < HEAP_MAX_SIZE - 4 - 8 - 4);

    uint32_t alloc_blocksize = 0;
    uint64_t payload_header = search_free_block(size, &alloc_blocksize);
    uint64_t payload_vaddr = NIL;

    if (payload_header != NIL)
    {
        payload_vaddr = try_alloc_with_splitting(payload_header, alloc_blocksize);
        assert(payload_vaddr != NIL);
    }
    else
    {
        payload_vaddr = try_extend_heap_to_alloc(alloc_blocksize);
    }

#ifdef DEBUG_MALLOC
    check_heap_correctness();
    check_free_block();
#endif

    return payload_vaddr;
}

void mem_free(uint64_t payload_vaddr)
{
    if (payload_vaddr == NIL)
    {
        return;
    }

    assert(get_firstblock() < payload_vaddr && payload_vaddr < get_epilogue());
    assert((payload_vaddr & 0x7) == 0x0);

    // request can be first or last block
    uint64_t req = get_header(payload_vaddr);
    uint64_t req_footer = get_footer(req); // for last block, it's 0

    uint32_t req_allocated = get_allocated(req);
    uint32_t req_blocksize = get_blocksize(req);
    assert(req_allocated == ALLOCATED); // otherwise it's free twice

    // block starting address of next & prev blocks
    uint64_t next = get_nextheader(req);
    uint64_t prev = get_prevheader(req);

    uint32_t next_allocated = get_allocated(next);
    uint32_t prev_allocated = get_allocated(prev);

    if (next_allocated == ALLOCATED && prev_allocated == ALLOCATED)
    {
        // case 1: *A(A->F)A*
        // ==> *AFA*
        set_allocated(req, FREE);
        set_allocated(req_footer, FREE);

        insert_free_block(req);
#ifdef DEBUG_MALLOC
        check_heap_correctness();
        check_free_block();
#endif
    }
    else if (next_allocated == FREE && prev_allocated == ALLOCATED)
    {
        // case 2: *A(A->F)FA
        // ==> *AFFA ==> *A[FF]A merge current and next
        delete_free_block(next);

        uint64_t one_free  = merge_blocks_as_free(req, next);
        
        insert_free_block(one_free);
#ifdef DEBUG_MALLOC
        check_heap_correctness();
        check_free_block();
#endif
    }
    else if (next_allocated == ALLOCATED && prev_allocated == FREE)
    {
        // case 3: AF(A->F)A*
        // ==> AFFA* ==> A[FF]A* merge current and prev
        delete_free_block(prev);

        uint64_t one_free  = merge_blocks_as_free(prev, req);
        
        insert_free_block(one_free);
#ifdef DEBUG_MALLOC
        check_heap_correctness();
        check_free_block();
#endif
    }
    else if (next_allocated == FREE && prev_allocated == FREE)
    {
        // case 4: AF(A->F)FA
        // ==> AFFFA ==> A[FFF]A merge current and prev and next
        delete_free_block(prev);
        delete_free_block(next);

        uint64_t one_free = merge_blocks_as_free(merge_blocks_as_free(prev, req), next);
        
        insert_free_block(one_free);
#ifdef DEBUG_MALLOC
        check_heap_correctness();
        check_free_block();
#endif
    }
    else
    {
#ifdef DEBUG_MALLOC
        printf("exception for free\n");
        exit(0);
#endif
    }
}




/* ------------------------------------- */
/*  Debugging and Correctness Checking   */
/* ------------------------------------- */

void check_heap_correctness()
{
    int linear_free_counter = 0;
    uint64_t p = get_firstblock();
    while(p != NIL && p <= get_lastblock())
    {
        assert(p % 8 == 4);
        assert(get_firstblock() <= p && p <= get_lastblock());

        uint64_t f = get_footer(p);
        uint32_t blocksize = get_blocksize(p);
        if (blocksize != 8)
        {
            assert(get_blocksize(p) == get_blocksize(f));
            assert(get_allocated(p) == get_allocated(f));
        }

        // rule 1: block[0] ==> A/F
        // rule 2: block[-1] ==> A/F
        // rule 3: block[i] == A ==> block[i-1] == A/F && block[i+1] == A/F
        // rule 4: block[i] == F ==> block[i-1] == A && block[i+1] == A
        // these 4 rules ensures that
        // adjacent free blocks are always merged together
        // henceforth external fragmentation are minimized
        if (get_allocated(p) == FREE)
        {
            linear_free_counter += 1;
        }
        else
        {
            linear_free_counter = 0;
        }
        assert(linear_free_counter <= 1);

        p = get_nextheader(p);
    }
}

static void block_info_print(uint64_t h)
{
    uint32_t a = get_allocated(h);
    uint32_t s = get_blocksize(h);
    uint64_t f = get_footer(h);

    uint32_t hv = *(uint32_t *)&heap[h];
    uint32_t fv = *(uint32_t *)&heap[f];

    uint32_t p8 = (hv >> 1) & 0x1;
    uint32_t b8 = (hv >> 2) & 0x1;
    uint32_t rb = (fv >> 1) & 0x1;

    printf("H:%lu,\tF:%lu,\tS:%u,\t(A:%u,RB:%u,B8:%u,P8:%u)\n", h, f, s, a, rb, b8, p8);
}

static void heap_blocks_print()
{
    printf("============\nheap blocks:\n");
    uint64_t h = get_firstblock();
    int i = 0;
    while (i < (HEAP_MAX_SIZE / 8) && h != NIL && h < get_epilogue())
    {
        block_info_print(h);
        h = get_nextheader(h);
    }
    printf("============\n");
}












//----------------------------------------------------------------


static int bst_heap_init();
static uint64_t bst_mem_alloc(uint32_t size);
static void bst_mem_free(uint64_t payload_vaddr);



// to allocate one physical page for heap
void os_syscall_brk(uint64_t start_vaddr){

}

// heap's bytes range:
// [heap_start_vaddr, heap_end_vaddr) or [heap_start_vaddr, heap_end_vaddr - 1]
// [0,1,2,3] - unused
// [4,5,6,7,8,9,10,11] - prologue block
// [12, ..., 4096 * n - 5] - regular blocks
// 4096 * n + [- 4, -3, -2, -1] - epilogue block (header only)
uint64_t heap_start_vaddr = 0;  // for unit test convenience
uint64_t heap_end_vaddr = 4096;


#define HEAP_MAX_SIZE (4096 * 8)
uint8_t heap[HEAP_MAX_SIZE];

static uint64_t round_up(uint64_t x, uint64_t n);

static uint32_t get_blocksize(uint64_t header_vaddr);
static void set_blocksize(uint64_t header_vaddr, uint32_t blocksize);

static uint32_t get_allocated(uint64_t header_vaddr);
static void set_allocated(uint64_t header_vaddr, uint32_t allocated);

static uint64_t get_prologue();
static uint64_t get_epilogue();

static uint64_t get_firstblock();
static uint64_t get_lastblock();

static int is_firstblock(uint64_t vaddr);
static int is_lastblock(uint64_t vaddr);

static uint64_t get_payload_addr(uint64_t vaddr);
static uint64_t get_header_addr(uint64_t vaddr);
static uint64_t get_footer_addr(uint64_t vaddr);

static uint64_t get_nextheader(uint64_t vaddr);
static uint64_t get_prevheader(uint64_t vaddr);

static void print_heap();

static int check_block(uint64_t header_vaddr);
static void check_heap_blocks();

// Round up to next multiple of n
// if (x == k * n)
// return x
// else, x = k * n + m and m < n
// return (k + 1) * n
// x 向 n 对齐
static uint64_t round_up(uint64_t x, uint64_t n){
    return n * ((x + n - 1) / n);
}


// applicapable for both header & footer
static uint32_t get_blocksize(uint64_t header_vaddr){
    
    if (header_vaddr == 0){
        return 0;
    }

    assert(get_prologue() <= header_vaddr && header_vaddr <= get_epilogue());
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert((header_vaddr & 0x3) == 0x0);//四字节对齐

    uint32_t header_value = *(uint32_t *)&heap[header_vaddr];
    return header_value & 0xFFFFFFF8;
}

static void set_blocksize(uint64_t header_vaddr, uint32_t blocksize){


    if (header_vaddr == 0){
        return;
    }

    assert(get_prologue() <= header_vaddr && header_vaddr <= get_epilogue()); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert((header_vaddr & 0x3) == 0x0);//四字节对齐
    assert((blocksize & 0x7) == 0x0);// blocksize should be 8 bytes aligned
    *(uint32_t *)&heap[header_vaddr] &= 0x00000007;
    *(uint32_t *)&heap[header_vaddr] |= blocksize;

}


// applicapable for both header & footer
static uint32_t get_allocated(uint64_t header_vaddr){

    if (header_vaddr == 0){
        // NULL can be considered as allocated
        return 1;
    }

    assert(get_prologue() <= header_vaddr && header_vaddr <= get_epilogue());
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert((header_vaddr & 0x3) == 0x0);//四字节对齐
    
    uint32_t header_value = *(uint32_t *)&heap[header_vaddr];
    return header_value & 0x1;
}

static void set_allocated(uint64_t header_vaddr, uint32_t allocated){

    if (header_vaddr == 0){
        return;
    }

    assert(get_prologue() <= header_vaddr && header_vaddr <= get_epilogue()); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert((header_vaddr & 0x3) == 0x0);//四字节对齐

    *(uint32_t *)&heap[header_vaddr] &= 0xFFFFFFF8;//对低三位清零
    *(uint32_t *)&heap[header_vaddr] |= (allocated & 0x1);

}

static uint64_t get_firstblock()
{
    assert(heap_end_vaddr > heap_start_vaddr);
    assert((heap_end_vaddr - heap_start_vaddr) % 4096 == 0);
    assert(heap_start_vaddr % 4096 == 0);

    // 4 for the not in use
    // 8 for the prologue block
    return get_prologue() + 8;
}

static uint64_t get_lastblock()
{
    assert(heap_end_vaddr > heap_start_vaddr);
    assert((heap_end_vaddr - heap_start_vaddr) % 4096 == 0);
    assert(heap_start_vaddr % 4096 == 0);

    uint64_t epilogue_header = get_epilogue();
    uint64_t last_footer = epilogue_header - 4;
    uint32_t last_blocksize = get_blocksize(last_footer);

    uint64_t last_header = epilogue_header - last_blocksize;

    assert(get_firstblock() <= last_header);

    return last_header;
}

static uint64_t get_prologue()
{
    assert(heap_end_vaddr > heap_start_vaddr);
    assert((heap_end_vaddr - heap_start_vaddr) % 4096 == 0);
    assert(heap_start_vaddr % 4096 == 0);

    // 4 for the not in use
    return heap_start_vaddr + 4;
}

static uint64_t get_epilogue()
{
    assert(heap_end_vaddr > heap_start_vaddr);
    assert((heap_end_vaddr - heap_start_vaddr) % 4096 == 0);
    assert(heap_start_vaddr % 4096 == 0);

    // epilogue block is having header only
    return heap_end_vaddr - 4;
}

static int is_firstblock(uint64_t vaddr)
{
    if (vaddr == 0)
    {
        return 0;
    }

    // vaddr can be:
    // 1. starting address of the block (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert(get_firstblock() <= vaddr && vaddr < get_epilogue());
    assert((vaddr & 0x3) == 0x0);

    uint64_t header_vaddr = get_header_addr(vaddr);

    if (header_vaddr == get_firstblock())
    {
        // it is the last block
        // it does not have any footer
        return 1;
    }

    // no, it's not the last block
    // it should have footer
    return 0;
}

static int is_lastblock(uint64_t vaddr){

    if (vaddr  == 0){
        return 0;
    }
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert(get_firstblock() <= vaddr && vaddr < get_epilogue());
    assert((vaddr & 0x3) == 0x0);

    uint64_t header_vaddr = get_header_addr(vaddr);
    uint32_t blocksize = get_blocksize(header_vaddr);

    // for last block, the virtual block size is still 8n
    // we imagine there is a footer in another physical page 
    // but it actually does not exist
    if (header_vaddr + blocksize == get_epilogue()){

        // it is the last block
        // it does not have any footer
        return 1;
    }

    // no, it's not the last block
    // it should have footer
    return 0;

}

static uint64_t get_payload_addr(uint64_t vaddr){
    
    if (vaddr == 0)
    {
        return 0;
    }    
    assert(get_firstblock() <= vaddr && vaddr < get_epilogue());
    
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert((vaddr & 0x3) == 0);

    // this round up will handle `vaddr == 0` situation

    return round_up(vaddr, 8);
}

static uint64_t get_header_addr(uint64_t vaddr){
    
    if (vaddr == 0)
    {
        return 0;
    }    
    assert(get_firstblock() <= vaddr && vaddr < get_epilogue());
    
    
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert((vaddr & 0x3) == 0);
    uint64_t payload_vaddr = get_payload_addr(vaddr);

    // NULL block does not have header
    return payload_vaddr == 0 ? 0 : payload_vaddr - 4;
}

static uint64_t get_footer_addr(uint64_t vaddr){
    
    if (vaddr == 0)
    {
        return 0;
    }    
    assert(get_firstblock() <= vaddr && vaddr < get_epilogue());
    
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert((vaddr & 0x3) == 0);

    uint64_t header_vaddr = get_header_addr(vaddr);
    uint64_t footer_vaddr = header_vaddr + get_blocksize(header_vaddr) - 4;

    assert(get_firstblock() < footer_vaddr && footer_vaddr < get_epilogue());
    return footer_vaddr;
}

static uint64_t get_nextheader(uint64_t vaddr){

    if (vaddr == 0 || is_lastblock(vaddr)){
        return 0;
    }

    assert(get_firstblock() <= vaddr && vaddr < get_lastblock());
    
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    uint64_t header_vaddr = get_header_addr(vaddr);
    // 将vaddr与8字节进行对齐，再减去4得到header的地址
    uint32_t block_size = get_blocksize(header_vaddr);
    uint64_t next_header_vaddr = header_vaddr + block_size;

    assert(get_firstblock() < next_header_vaddr && next_header_vaddr <= get_lastblock());
    // 确保在最末尾
    // 至少空余出一个8字节的payload + 4字节的header   故减12
    // 否则next_header_vaddr就没有意义了
    return next_header_vaddr;

}

static uint64_t get_prevheader(uint64_t vaddr){


    if (vaddr == 0 || is_firstblock(vaddr))
    {
        return 0;
    }
    assert(get_firstblock() < vaddr && vaddr <= get_lastblock());
    

    uint64_t header_vaddr = get_header_addr(vaddr);
    // 将vaddr与8字节进行对齐，再减去4得到header的地址


    uint64_t prev_footer_vaddr = header_vaddr - 4;

    uint32_t block_size = get_blocksize(prev_footer_vaddr);
    uint64_t prev_header_vaddr = header_vaddr - block_size;

    assert(get_firstblock() <= prev_header_vaddr && prev_header_vaddr <= get_lastblock());
    // 至少空余出一个8字节的payload + 4字节的header   故减16
    // 否则next_header_vaddr就没有意义了
    return prev_header_vaddr;

}


/**************************************************/
/*  Explicit Free List                            */
/**************************************************/


static uint32_t get_nextfree(uint64_t header_vaddr){
    
    return *(uint32_t *)&heap[header_vaddr + 8];
}

static uint32_t get_prevfree(uint64_t header_vaddr){
    
    return *(uint32_t *)&heap[header_vaddr + 4];
}

static int check_block(uint64_t header_vaddr){
    // rule 1: block[0] ==> A/F
    // rule 2: block[-1] ==> A/F
    // rule 3: block[i] == A ==> block[i-1] == A/F && block[i+1] == A/F
    // rule 4: block[i] == F ==> block[i-1] == A && block[i+1] == A
    // these 4 rules ensures that
    // adjacent free blocks are always merged together
    // henceforth external fragmentation are minimized
    assert(header_vaddr % 8 == 4);
    
    if (get_allocated(header_vaddr) == 1){
        // applies rule 3
        return 1;
    }
    uint32_t prev_allocated = 1;
    uint32_t next_allocated = 1;
    
    if (header_vaddr == heap_start_vaddr){
        // the first block. there is no prev block
        // applies rule 1
        prev_allocated = 1;
    }
    else{
        prev_allocated = get_allocated(get_prevheader(header_vaddr));
    }
    if (is_lastblock(header_vaddr) == 1){
        // the last block. there is no next block
        // applies rule 2
        next_allocated = 1;
    }
    else{
        next_allocated = get_allocated(get_nextheader(header_vaddr));
    }
    // applies rule 4
    // current block is free
    if (prev_allocated == 1 && next_allocated == 1){
        return 1;
    }
    return 0;
}

static void check_heap_blocks()
{
    int linear_free_counter = 0;
    uint64_t p = get_firstblock();
    while(p != 0 && p <= get_lastblock())
    {
        assert(p % 8 == 4);
        assert(get_firstblock() <= p && p <= get_lastblock());

        assert(*(uint32_t *)&heap[p] == *(uint32_t *)&heap[get_footer_addr(p)]);

        // rule 1: block[0] ==> A/F
        // rule 2: block[-1] ==> A/F
        // rule 3: block[i] == A ==> block[i-1] == A/F && block[i+1] == A/F
        // rule 4: block[i] == F ==> block[i-1] == A && block[i+1] == A
        // these 4 rules ensures that
        // adjacent free blocks are always merged together
        // henceforth external fragmentation are minimized
        if (get_allocated(p) == 0)
        {
            linear_free_counter += 1;
        }
        else
        {
            linear_free_counter = 0;
        }
        assert(linear_free_counter <= 1);

        p = get_nextheader(p);
    }
}


int heap_init(){
    // reset all to 0
    for (int i = 0; i < HEAP_MAX_SIZE / 8; i += 8)
    {
        *(uint64_t *)&heap[i] = 0;
    }
    // heap_start_vaddr is the starting address of the first block
    // the payload of the first block is 8B aligned ([8])
    // so the header address of the first block is [8] - 4 = [4]
    heap_start_vaddr = 0;
    heap_end_vaddr = 4096;

    // set the prologue block
    uint64_t prologue_header = get_prologue();
    set_blocksize(prologue_header, 8);
    set_allocated(prologue_header, 1);

    uint64_t prologue_footer = prologue_header + 4;
    set_blocksize(prologue_footer, 8);
    set_allocated(prologue_footer, 1);

    // set the epilogue block
    // it's a footer only
    uint64_t epilogue = get_epilogue();
    set_blocksize(epilogue, 0);
    set_allocated(epilogue, 1);

    // set the block size & allocated of the only regular block
    uint64_t first_header = get_firstblock();
    set_blocksize(first_header, 4096 - 4 - 8 - 4);
    set_allocated(first_header, 0);

    uint64_t first_footer = get_footer_addr(first_header);
    set_blocksize(first_footer, 4096 - 4 - 8 - 4);
    set_allocated(first_footer, 0);

    return 0;
}


static uint64_t try_alloc(uint64_t block_vaddr, uint32_t request_blocksize){

    uint64_t b = block_vaddr;
    uint32_t b_blocksize = get_blocksize(b);
    uint32_t b_allocated = get_allocated(b);

    if (b_allocated == 0 && b_blocksize >= request_blocksize){
        // allocate this block
        if (b_blocksize > request_blocksize){

            // split this block 'b'
            // b_blocksize - request_blocksize >= 8
            uint64_t next_footer = get_footer_addr(b);
            set_allocated(next_footer, 0);
            set_blocksize(next_footer, b_blocksize - request_blocksize);


            set_allocated(b, 1);
            set_blocksize(b, request_blocksize);

            uint64_t b_footer = get_footer_addr(b);
            set_allocated(b_footer, 1);
            set_blocksize(b_footer, request_blocksize);

            // set the left splitted block
            // in the extreme situation, next block size == 8
            // which makes the whole block of next to be:
            //[0x00000008, 0x00000008]
            uint64_t next_header = get_nextheader(b);
            set_allocated(next_header, 0);
            set_blocksize(next_header, b_blocksize - request_blocksize);

            assert(get_footer_addr(next_header) == next_footer);

            return get_payload_addr(b);
        }
        else {
            // no need to split this block
            // set_blocksize(b, request_blocksize)
            set_allocated(b, 1);
            return get_payload_addr(b);
        }
    }
    return 0;
}



// size ----- requested payload size
// return --- the virtual address of payload
uint64_t mem_alloc(uint32_t size){
    
    assert(0 < size && size < HEAP_MAX_SIZE - 4 - 8 - 4);

    uint64_t payload_vaddr = 0;

    uint32_t request_blocksize = round_up(size, 8) + 4 + 4;



    uint64_t b = get_firstblock();
    uint64_t epilogue = get_epilogue();

    

    while(b != 0 && b < epilogue){

        payload_vaddr = try_alloc(b, request_blocksize);

        if (payload_vaddr != 0){
            return payload_vaddr;
        }
        else {
            // go to next block

            b = get_nextheader(b);
        }
    }

    // when no enough free block for current heap
    // request a new free physical & virtual page from OS
    while (heap_end_vaddr + 4096 <= HEAP_MAX_SIZE){

        uint64_t last_header = get_lastblock();

        // we can allocate one page for the request
        uint64_t old_epilogue = epilogue;

        // brk system call
        os_syscall_brk(heap_end_vaddr);
        heap_end_vaddr += 4096;

        // set epilogue
        epilogue = get_epilogue();
        set_allocated(epilogue, 1);
        set_blocksize(epilogue, 0);

        uint32_t last_allocated = get_allocated(last_header);
        uint32_t last_blocksize = get_blocksize(last_header);
        if (last_allocated == 1){

            // no merging is needed

            // take place the old epilogue
            set_allocated(old_epilogue, 0);
            set_blocksize(old_epilogue, 4096);

            // set the new footer
            set_allocated(epilogue - 4, 0);
            set_blocksize(epilogue - 4, 4096);

        }
        else {
            // merge with last_block is needed
            set_allocated(last_header, 0);
            set_blocksize(last_header, last_blocksize + 4096);

            uint64_t last_footer = get_footer_addr(last_header);
            set_allocated(last_footer, 0);
            set_blocksize(last_footer, last_blocksize + 4096);
        }
        // try to allocate
        payload_vaddr = try_alloc(last_header, request_blocksize);

        if (payload_vaddr != 0){
            return payload_vaddr;
        }
        // else, continue to request page from OS
    }


#ifdef DEBUG_MALLOC
    printf("OS cannot allocate physical page for heap!\n");
#endif

    // <==> return NULL;
    return 0;
}



void mem_free(uint64_t payload_vaddr){

     
    assert(get_firstblock() < payload_vaddr && payload_vaddr < get_epilogue());
    assert((payload_vaddr & 0x7) == 0x0);// 8 bytes aligned
    // request can be first or last block
    uint64_t req = get_header_addr(payload_vaddr);
    uint64_t req_footer = get_footer_addr(req);// for last block, it's 0

    uint32_t req_allocated = get_allocated(req);
    uint32_t req_blocksize = get_blocksize(req);

    assert(req_allocated == 1); // otherwise it's free twice

    // block starting address of next & prev blocks
    // TODO: conrner case -- req can be the first or last block
    uint64_t next = get_nextheader(payload_vaddr);  // for req lsat block, it's 0
    uint64_t prev = get_prevheader(payload_vaddr);  // for req first block, it's 0
    uint64_t next_footer = get_footer_addr(next);

    uint32_t next_allocated = get_allocated(next);  // for req last, 1
    uint32_t prev_allocated = get_allocated(prev);  // for req first, 1

    uint32_t next_blocksize = get_blocksize(next);  // for req last, 0
    uint32_t prev_blocksize = get_blocksize(prev);  // for req first, 0

    if (next_allocated == 1 && prev_allocated == 1){
        
        // case 1: *A(A->F)A*
        // ==> *AFA*
        set_allocated(req, 0);
        set_allocated(req_footer, 0);

    }
    else if (next_allocated == 0 && prev_allocated == 1){

        // case 2: *A(A->F)FA
        // ==> *AFFA ==> *A[FF]A merge current and next
        set_allocated(req, 0);
        set_blocksize(req, req_blocksize + next_blocksize);

        set_allocated(next_footer, 0);
        set_blocksize(next_footer, req_blocksize + next_blocksize);

    }
    else if (next_allocated == 1 && prev_allocated == 0){

        //case 3: AF(A->F)A*
        // ==> AFFA*   ==> A[FF]A* merge current and prev
        set_allocated(prev, 0);
        set_blocksize(prev, prev_blocksize + req_blocksize);

        set_allocated(req_footer, 0);
        set_blocksize(req_footer, prev_blocksize + req_blocksize);
    }
    else if (next_allocated == 0 && prev_allocated == 0){

        //case 4: AF(A->F)FA
        // ==> AFFFA   ==> A[FFF]A merge current and prev and next
        set_allocated(prev, 0);
        set_blocksize(prev, prev_blocksize + req_blocksize + next_blocksize);

        set_allocated(next_footer, 0);
        set_blocksize(next_footer, prev_blocksize + req_blocksize + next_blocksize);
    }
    else{
#ifdef DEBUG_MALLOC
        printf("exception for free\n");
        exit(0);
#endif  
    }  
}




#ifdef DEBUG_MALLOC

static void test_roundup(){
    printf("Testing round up ...\n");
    for (int i = 0; i < 100; ++ i){
        for (int j = 1; j <= 8; ++ j){
            uint32_t x = i * 8 + j;
            assert(round_up(x, 8) == (i + 1) * 8);
        }
    }
    printf("\033[32;1m\tPass\033[0m\n");
}
/*  hex table
    0       0000    *
    1       0001    *
    2       0010
    3       0011
    4       0100
    5       0101
    6       0110
    7       0111
    8       1000    *
    9       1001    *
    a   10  1010
    b   11  1011
    c   12  1100
    d   13  1101
    e   14  1110
    f   15  1111
 */
static void test_get_blocksize_allocated(){
    printf("Testing getting block size from header ...\n");

    for (int i = get_prologue(); i <= get_epilogue(); i += 4){
        *(uint32_t *)&heap[i] = 0x1234abc0;
        assert(get_blocksize(i) == 0x1234abc0);
        assert(get_allocated(i) == 0);
        *(uint32_t *)&heap[i] = 0x1234abc1;
        assert(get_blocksize(i) == 0x1234abc0);
        assert(get_allocated(i) == 1);
        *(uint32_t *)&heap[i] = 0x1234abc8;
        assert(get_blocksize(i) == 0x1234abc8);
        assert(get_allocated(i) == 0);
        *(uint32_t *)&heap[i] = 0x1234abc9;
        assert(get_blocksize(i) == 0x1234abc8);
        assert(get_allocated(i) == 1);
    }
    printf("\033[32;1m\tPass\033[0m\n");
}
static void test_set_blocksize_allocated(){
    printf("Testing setting block size to header ...\n");

    for (int i = get_prologue(); i <= get_epilogue(); i += 4){
        set_blocksize(i, 0x1234abc0);
        set_allocated(i, 0);
        assert(get_blocksize(i) == 0x1234abc0);
        assert(get_allocated(i) == 0);
        set_blocksize(i, 0x1234abc0);
        set_allocated(i, 1);
        assert(get_blocksize(i) == 0x1234abc0);
        assert(get_allocated(i) == 1);
        set_blocksize(i, 0x1234abc8);
        set_allocated(i, 0);
        assert(get_blocksize(i) == 0x1234abc8);
        assert(get_allocated(i) == 0);
        set_blocksize(i, 0x1234abc8);
        set_allocated(i, 1);
        assert(get_blocksize(i) == 0x1234abc8);
        assert(get_allocated(i) == 1);
    }
    // set the block size of last block
    for (int i = 2; i < 100; ++ i){
        uint32_t blocksize = i * 8;
        uint64_t addr = get_epilogue() - blocksize;   // + 4 for the virtual footer in next page
        set_blocksize(addr, blocksize);
        assert(get_blocksize(addr) == blocksize);
        assert(is_lastblock(addr) == 1);
    }
    printf("\033[32;1m\tPass\033[0m\n");
}
static void test_get_header_payload_addr(){
    printf("Testing getting header or payload virtual addresses ...\n");

    uint64_t header_addr, payload_addr;
    for (int i = get_payload_addr(get_firstblock()); i < get_epilogue(); i += 8){
        payload_addr = i;
        header_addr = payload_addr - 4;
        assert(get_payload_addr(header_addr) == payload_addr);
        assert(get_payload_addr(payload_addr) == payload_addr);
        assert(get_header_addr(header_addr) == header_addr);
        assert(get_header_addr(payload_addr) == header_addr);
    }
    printf("\033[32;1m\tPass\033[0m\n");
}

static void print_heap(){
    printf("============\nheap blocks:\n");
    uint64_t h = get_firstblock();
    int i = 0;
    while (h != 0 && h < get_epilogue()){
        uint32_t a = get_allocated(h);
        uint32_t s = get_blocksize(h);
        uint64_t f = get_footer_addr(h);

        printf("[H:%lu,F:%lu,S:%u,A:%u]  ", h, f, s, a);
        h = get_nextheader(h);

        i += 1;
        if (i % 5 == 0){
            printf("\b\n");
        }
    }
    printf("\b\b\n============\n");
}

static void test_get_next_prev(){
    printf("Testing linked list traversal ...\n");

    srand(123456);

    // let me setup the heap first
    heap_init();

    uint64_t h = get_firstblock();
    uint64_t f = 0;

    uint32_t collection_blocksize[1000];
    uint32_t collection_allocated[1000];
    uint32_t collection_headeraddr[1000];
    int counter = 0;

    uint32_t allocated = 1;
    uint64_t epilogue = get_epilogue();
    while (h < epilogue){
        uint32_t blocksize = 8 * (1 + rand() % 16);
        if (epilogue - h <= 64){
            blocksize = epilogue - h;
        }

        if (allocated == 1 && (rand() % 3) >= 1){
            // with previous allocated, 2/3 possibilities to be free
            allocated = 0;
        }
        else{
            allocated = 1;
        }

        collection_blocksize[counter] = blocksize;
        collection_allocated[counter] = allocated;
        collection_headeraddr[counter] = h;
        counter += 1;

        set_blocksize(h, blocksize);
        set_allocated(h, allocated);

        f = h + blocksize - 4;
        set_blocksize(f, blocksize);
        set_allocated(f, allocated);

        h = h + blocksize;
    }

    // check get_next
    h = get_firstblock();
    int i = 0;
    while (h != 0 && h < get_epilogue()){
        assert(i <= counter);
        assert(h == collection_headeraddr[i]);
        assert(get_blocksize(h) == collection_blocksize[i]);
        assert(get_allocated(h) == collection_allocated[i]);
        assert(check_block(h) == 1);
        
        h = get_nextheader(h);
        i += 1;
    }

    // check get_prev
    h = get_lastblock();
    i = counter - 1;
    while (h != 0 && get_firstblock() <= h){
        assert(0 <= i);
        assert(h == collection_headeraddr[i]);
        assert(get_blocksize(h) == collection_blocksize[i]);
        assert(get_allocated(h) == collection_allocated[i]);

        h = get_prevheader(h);
        i -= 1;
    }
    printf("\033[32;1m\tPass\033[0m\n");
}

// static void test_implicit_list(){
//     printf("Testing implicit list malloc & free ...\n");

//     heap_init();

//     srand(123456);

//     // collection for the pointers
//     linkedlist_t *ptrs = linkedlist_construct();

//     for (int i = 0; i < 10; ++ i){
//         uint32_t size = rand() % 1024 + 1;  // a non zero value

//         if ((rand() & 0x1) == 0){
//             // malloc
//             printf("\t[%d]\tmalloc(%u)", i, size);
//             uint64_t p = mem_alloc(size);
//             ptrs = linkedlist_add(ptrs, p);
//             printf("\t%lu\n", p);
//         }
//         else if (ptrs->count != 0){
//             // free
//             // randomly select one to free
//             linkedlist_node_t *t = linkedlist_get(ptrs, rand() % ptrs->count);
//             printf("\t[%d]\tfree(%lu)\n", i, t->value);
//             mem_free(t->value);
//             linkedlist_delete(ptrs, t);
//         }

//         print_heap();
//         printf("(");
//         for (int k = 0; k < ptrs->count; ++ k){
//             printf("%lu, ", linkedlist_next(ptrs)->value);
//         }
//         printf("\b\b)\n");
//     }

//     printf("\033[32;1m\tPass\033[0m\n");
// }
int main(){
    test_roundup();
    test_get_blocksize_allocated();
    test_set_blocksize_allocated();
    test_get_header_payload_addr();
    test_get_next_prev();
    
    // test_implicit_list();
    return 0;
}





#endif