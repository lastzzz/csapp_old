#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../../header/cpu.h"
#include "../../header/memory.h"
#include "../../header/common.h"


uint64_t va2pa(uint64_t vaddr, core_t *cr){


    return vaddr % PHYSICAL_MEMORY_SPACE;
    // return vaddr & (0xffffffffffffffff >> (64 - MAX_NUM_PHYSICAL_PAGE));

    //等价于 vaddr & 0xfffff  <=> vaddr % 65536
}

 