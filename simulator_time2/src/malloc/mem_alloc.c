#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include "../header/allocator.h"


int heap_init();
uint64_t mem_alloc(uint32_t size);
void mem_free(uint64_t vaddr);

// to allocate one physical page for heap
void os_syscall_brk(uint64_t start_vaddr){

}


uint64_t heap_start_vaddr = 0;
uint64_t heap_end_vaddr = 0;

#define HEAP_MAX_SIEZE (4096 * 8)
uint8_t heap[HEAP_MAX_SIEZE];

static uint64_t round_up(uint64_t x, uint64_t n);

static uint32_t get_blocksize(uint64_t header_vaddr);
static void set_blocksize(uint64_t header_vaddr, uint32_t blocksize);

static uint32_t get_allocated(uint64_t header_vaddr);
static void set_allocated(uint64_t header_vaddr, uint32_t allocated);

static int is_lastblock(uint64_t vaddr);

static uint64_t get_payload_addr(uint64_t vaddr);
static uint64_t get_header_addr(uint64_t vaddr);
static uint64_t get_footer_addr(uint64_t vaddr);

static uint64_t get_nextheader(uint64_t vaddr);
static uint64_t get_prevheader(uint64_t vaddr);

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


    assert(heap_start_vaddr <= header_vaddr && header_vaddr <= heap_end_vaddr); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert(header_vaddr & 0x3 == 0x0);//四字节对齐

    uint32_t header_value = *(uint32_t *)&heap[header_vaddr];
    return header_value & 0xFFFFFFF8;
}

static void set_blocksize(uint64_t header_vaddr, uint32_t blocksize){


    if (header_vaddr == 0){
        return;
    }

    assert(heap_start_vaddr <= header_vaddr && header_vaddr <= heap_end_vaddr); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert(header_vaddr & 0x3 == 0x0);//四字节对齐
    assert(blocksize & 0x7 == 0x0);// blocksize should be 8 bytes aligned
    *(uint32_t *)&heap[header_vaddr] &= 0x00000007;
    *(uint32_t *)&heap[header_vaddr] |= blocksize;

}


// applicapable for both header & footer
static uint32_t get_allocated(uint64_t header_vaddr){

    if (header_vaddr == 0){
        // NULL can be considered as allocated
        return 1;
    }

    assert(heap_start_vaddr <= header_vaddr && header_vaddr <= heap_end_vaddr); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert(header_vaddr & 0x3 == 0x0);//四字节对齐
    
    uint32_t header_value = *(uint32_t *)&heap[header_vaddr];
    return header_value & 0x1;
}

static void set_allocated(uint64_t header_vaddr, uint32_t allocated){

    if (header_vaddr == 0){
        return;
    }

    assert(heap_start_vaddr <= header_vaddr && header_vaddr <= heap_end_vaddr); 
    // 至少空余出4字节的footer
    // 4 bytes alignment
    assert(header_vaddr & 0x3 == 0x0);//四字节对齐

    *(uint32_t *)&heap[header_vaddr] &= 0xFFFFFFF8;//对低三位清零
    *(uint32_t *)&heap[header_vaddr] |= (allocated & 0x1);

}

static int is_lastblock(uint64_t vaddr){

    if (vaddr  == 0){
        return 0;
    }
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    assert(heap_start_vaddr <= vaddr && vaddr <= heap_end_vaddr);
    assert((vaddr & 0x3) == 0x0);

    uint64_t header_vaddr = get_header_addr(vaddr);
    uint32_t blocksize = get_blocksize(header_vaddr);

    // for last block, the virtual block size is still 8n
    // we imagine there is a footer in another physical page 
    // but it actually does not exist
    if (header_vaddr + blocksize == heap_end_vaddr + 1 + 4){

        // it is the last block
        // it does not have any footer
        return 1;
    }

    // no, it's not the last block
    // it should have footer
    return 0;

}

static uint64_t get_payload_addr(uint64_t vaddr){
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    return round_up(vaddr, 8);
}

static uint64_t get_header_addr(uint64_t vaddr){
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    uint64_t payload_vaddr = get_payload_addr(vaddr);

    // NULL block does not have header
    return payload_vaddr == 0 ? 0 : payload_vaddr - 4;
}

static uint64_t get_footer_addr(uint64_t vaddr){
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    uint64_t next_header = get_nextheader(vaddr);

    // last block does not have footer
    return  next_header == 0 ? 0 : next_header - 4;
}

static uint64_t get_nextheader(uint64_t vaddr){

    if (vaddr == 0 || is_lastblock(vaddr)){
        return 0;
    }

    assert(heap_start_vaddr <= vaddr && vaddr <= heap_end_vaddr);
    
    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    uint64_t header_vaddr = get_header_addr(vaddr);
    // 将vaddr与8字节进行对齐，再减去4得到header的地址
    uint32_t block_size = get_blocksize(header_vaddr);
    uint64_t next_header_vaddr = header_vaddr + block_size;

    assert(heap_start_vaddr <= next_header_vaddr && next_header_vaddr <= heap_end_vaddr);
    // 确保在最末尾
    // 至少空余出一个8字节的payload + 4字节的header   故减12
    // 否则next_header_vaddr就没有意义了
    return next_header_vaddr;

}

static uint64_t get_prevheader(uint64_t vaddr){




    if (vaddr == 0){
        return 0;
    }

    // vaddr can be:
    // 1. starting address of th block  (8 * n + 4)
    // 2. starting address of the payload (8 * m)
    uint64_t header_vaddr = get_header_addr(vaddr);
    // 将vaddr与8字节进行对齐，再减去4得到header的地址

    if (header_vaddr == heap_start_vaddr){
        // this block is the first block in heap 
        return 0;
    }
    
    assert(header_vaddr >= 16);
    // 确保在最开头
    // 至少空余出一个8字节的payload + 4字节的header + 4字节的footer，故大于等于16

    uint64_t prev_footer_vaddr = header_vaddr - 4;

    uint32_t block_size = get_blocksize(prev_footer_vaddr);
    uint64_t prev_header_vaddr = header_vaddr - block_size;

    assert(heap_start_vaddr <= prev_header_vaddr && prev_header_vaddr <= heap_end_vaddr - 12);
    // 至少空余出一个8字节的payload + 4字节的header   故减16
    // 否则next_header_vaddr就没有意义了
    return prev_header_vaddr;

}




int heap_check(){
    // rule 1: block[0] ==> A/F
    // rule 2: block[-1] ==> A/F
    // rule 3: block[i] == A ==> block[i-1] == A/F && block[i+1] == A/F
    // rule 4: block[i] == F ==> block[i-1] == A && block[i+1] == A
    // these 4 rules ensures that
    // adjacent free blocks are always merged together
    // henceforth external fragmentation are minimized
    return 0;
}

int heap_init(){
    // heap_start_vaddr is the starting address of the first block
    // the payload of the first block is 8B aligned ([8])
    // so the header address of the first block is [8] - 4 = [4]
    heap_start_vaddr = 4;
    
    set_allocated(heap_start_vaddr, 0);
    set_blocksize(heap_start_vaddr, 4096 - 8);


    // we do not set footer for the last block in heap
    heap_end_vaddr = 4096 - 1;

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
            set_allocated(b, 1);
            set_blocksize(b, request_blocksize);

            // set the left splitted block
            // in the extreme situation, next block size == 8
            // which makes the whole block of next to be:
            //[0x00000008, 0x00000008]
            uint64_t next_header_vaddr = b + request_blocksize;
            set_allocated(next_header_vaddr, 0);
            set_blocksize(next_header_vaddr, b_blocksize - request_blocksize);

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
    
    assert(0 < size && size < 4096 - 8);

    uint64_t payload_vaddr = 0;

    uint32_t request_blocksize = round_up(size, 8) + 4 + 4;

    uint64_t last_block = 0;
    uint64_t b = heap_start_vaddr;

    

    while(b < heap_end_vaddr){

        payload_vaddr = try_alloc(b, request_blocksize);

        if (payload_vaddr != 0){
            return payload_vaddr;
        }
        else {
            // go to next block
            if (is_lastblock(b)){
                last_block = b;
            }
            b = get_nextheader(b);
        }
    }

    // when no enough free block for current heap
    // request a new free physical & virtual page from OS
    if (heap_end_vaddr + 1 + 4096 <= HEAP_MAX_SIEZE){

        uint64_t old_heap_end = heap_end_vaddr;
        // we can allocate one page for the request
        // brk system call
        os_syscall_brk(heap_end_vaddr + 1);
        heap_end_vaddr += 4096;


        uint32_t last_allocated = get_allocated(last_block);
        uint32_t last_blocksize = get_blocksize(last_block);
        if (last_allocated == 1){

            // no merging is needed

            // add the footer for last block
            set_allocated(old_heap_end, 1);
            set_blocksize(old_heap_end, last_blocksize);

            set_allocated(old_heap_end + 4, 0);
            set_blocksize(old_heap_end + 4, 4096);

            // update the last block
            last_block = old_heap_end + 4;

        }
        else {
            // merge with last_block is needed
            set_blocksize(last_block, last_blocksize + 4096);
        }
        // try to allocate
        payload_vaddr = try_alloc(last_block, request_blocksize);

        if (payload_vaddr != 0){
            return payload_vaddr;
        }

    }
    else{

#ifdef DEBUG_MALLOC
    printf("OS cannot allocate physical page for heap!\n");
#endif
    }

    // <==> return NULL;
    return 0;
}



void mem_free(uint64_t payload_vaddr){

     
    assert(heap_start_vaddr <= payload_vaddr && payload_vaddr <= heap_end_vaddr);
    assert(payload_vaddr & 0x7 == 0x0);// 8 bytes aligned
    // request can be first or last block
    uint64_t req = get_header_addr(payload_vaddr);
    uint64_t req_footer = get_footer_addr(req);// for last block, it's 0

    uint32_t req_allocated = get_allocated(req);
    uint32_t req_blocksize = get_blocksize(req);

    assert(req_allocated == 1);

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

    
#ifdef DEBUG_MALLOC
    printf("exception for free\n");
    exit(0);
#endif  
}




#ifdef DEBUG_MALLOC
int main(){
    printf("malloc!\n");
    heap_init();
    return 0;
}
#endif