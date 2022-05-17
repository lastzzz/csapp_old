#include "../../header/address.h"
#include "../../header/memory.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>



#define NUM_CACHE_LINE_PER_SET (8)

static void read_sram_cacheline(address_t paddr, uint8_t *block);
static void write_sram_cacheline(address_t paddr, uint8_t *block);


// write-back and write-allocate
typedef enum
{
    CACHE_LINE_INVALID,
    CACHE_LINE_CLEAN, // in MESI: E, S
    CACHE_LINE_DIRTY
} sram_cacheline_state_t;


// lines[ct]  cache tag
typedef struct 
{
    sram_cacheline_state_t state;
    int time;  // timer to find LRU line inside one set
    uint64_t tag;
    uint8_t block[(1 << SRAM_CACHE_OFFSET_LENGTH)];
} sram_cacheline_t;

// sets[ci] cache index
typedef struct
{
    sram_cacheline_t lines[NUM_CACHE_LINE_PER_SET];
} sram_cacheset_t;

// 一个cache
typedef struct
{
    sram_cacheset_t sets[(1 << SRAM_CACHE_INDEX_LENGTH)];
} sram_cache_t;

static sram_cache_t cache;




/* interface of I/O Bus: read and write between the SRAM cache and DRAM memory
 */

static void read_sram_cacheline(address_t paddr, uint8_t *block){

    uint64_t size64 = sizeof(uint64_t);

    uint64_t dram_base = ((paddr.paddr_value >> SRAM_CACHE_OFFSET_LENGTH) << SRAM_CACHE_TAG_LENGTH);
    uint64_t block_base = (uint64_t)block;

    for (int i = 0; i < ((1 << SRAM_CACHE_OFFSET_LENGTH) / size64); ++i){

        uint64_t dram_addr = dram_base + i * size64;
        uint64_t block_addr = block_base + i * size64;

        *(uint64_t *)(block_addr) = read64bits_dram(dram_addr);
    }

}


static void write_sram_cacheline(address_t paddr, uint8_t *block){
    
    uint64_t size64 = sizeof(uint64_t);

    uint64_t dram_base = ((paddr.paddr_value >> SRAM_CACHE_OFFSET_LENGTH) << SRAM_CACHE_TAG_LENGTH);
    uint64_t block_base = (uint64_t)block;

    for (int i = 0; i < ((1 << SRAM_CACHE_OFFSET_LENGTH) / size64); ++i){

        uint64_t dram_addr = dram_base + i * size64;
        uint64_t block_addr = block_base + i * size64;

        uint64_t value = *(uint64_t *)(block_addr);
        write64bits_dram(dram_addr, value);
    }

}

