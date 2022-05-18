#include "../../header/address.h"
#include "../../header/memory.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>



#define NUM_CACHE_LINE_PER_SET (8)



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




uint8_t sram_cache_read(uint64_t paddr_value){

    address_t paddr = {
        .paddr_value = paddr_value,
    };


    sram_cacheset_t set = cache.sets[paddr.ci];

    sram_cacheline_t *victim = NULL;
    sram_cacheline_t *invalid = NULL;
    int max_time = -1;

    // update LRU time
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; ++i){
        set.lines[i].time++;
        if (max_time < set.lines[i].time){
            // select this line as victim by LRU policy
            // replace it when all lines are valid
            victim = &(set.lines[i]);
            max_time = set.lines[i].time;
        }
        if (set.lines[i].state == CACHE_LINE_INVALID){
            //exist one invalid line as candidate for cache miss
            invalid = &(set.lines[i]);
        }
    }

    // try cache hit
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; ++i){
        sram_cacheline_t line = set.lines[i];

        if (line.state != CACHE_LINE_INVALID && line.tag == paddr.ct){

            // cache hit
            // update LRU time
            line.time = 0;
            // find the byte
            return line.block[paddr.co];
        }
    }

    // cache miss: load from memory


    //try to find one free cache line
    if (invalid != NULL){
        // load data from DRAM to this invalid cache line
        bus_read(paddr.paddr_value, &(invalid->block));

        //update cache line state
        invalid->state = CACHE_LINE_CLEAN;

        // update LRU
        invalid->time = 0;

        // update tag
        invalid->tag = paddr.ct;
        return invalid->block[paddr.co];
    }
    assert(victim != NULL);
    // no free cache line, use LRU policy
    if (victim->state == CACHE_LINE_DIRTY){
        // write back the dirty line to dram
        bus_write(paddr.paddr_value, victim);

        // update state
        victim->state = CACHE_LINE_INVALID;
    }

    // if CACHE_LINE_CLEAN discard this victim directly
    victim->state = CACHE_LINE_INVALID;

    // read from dram
    // load data from DRAM to this invalid cache line
    bus_read(paddr.paddr_value, &(victim->block));

    //update cache line state
    victim->state = CACHE_LINE_CLEAN;

    // update LRU
    victim->time = 0;

    // update tag
    victim->tag = paddr.ct;
    return victim->block[paddr.co];

}


void sram_cache_write(uint64_t paddr_value, uint8_t data){

    address_t paddr = {
        .paddr_value = paddr_value,
    };

    sram_cacheset_t set = cache.sets[paddr.ci];
    sram_cacheline_t *victim = NULL;
    sram_cacheline_t *invalid = NULL; // for write-allocate
    int max_time = -1;

    // update LRU time
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; ++i){
        set.lines[i].time++;
        if (max_time < set.lines[i].time){
            // select this line as victim by LRU policy
            // replace it when all lines are valid
            victim = &(set.lines[i]);
            max_time = set.lines[i].time;
        }
        if (set.lines[i].state == CACHE_LINE_INVALID){
            //exist one invalid line as candidate for cache miss
            invalid = &(set.lines[i]);
        }
    }

    // try cache hit
    for (int i = 0; i < NUM_CACHE_LINE_PER_SET; ++i){
        sram_cacheline_t line = set.lines[i];

        if (line.state != CACHE_LINE_INVALID && line.tag == paddr.ct){

            // cache hit

            // update LRU time
            line.time = 0;

            // find the byte
            line.block[paddr.co] = data;

            // update state
            line.state = CACHE_LINE_DIRTY;
            return;
        }
    }

    // cache miss: load from memory

    //write-allocate

    //try to find one free cache line
    if (invalid != NULL){
        // load data from DRAM to this invalid cache line
        bus_read(paddr.paddr_value, &(invalid->block));

        //update cache line state
        invalid->state = CACHE_LINE_DIRTY;

        // update LRU
        invalid->time = 0;

        // update tag
        invalid->tag = paddr.ct;
        
        // write data
        invalid->block[paddr.co] = data;
        return;
    }
    assert(victim != NULL);
    // no free cache line, use LRU policy
    if (victim->state == CACHE_LINE_DIRTY){
        // write back the dirty line to dram
        bus_write(paddr.paddr_value, victim);

        // update state
        victim->state = CACHE_LINE_INVALID;
    }

    // if CACHE_LINE_CLEAN discard this victim directly
    victim->state = CACHE_LINE_INVALID;

    // read from dram
    // write-allocate
    // load data from DRAM to this invalid cache line
    bus_read(paddr.paddr_value, &(victim->block));

    //update cache line state
    victim->state = CACHE_LINE_DIRTY;

    // update LRU
    victim->time = 0;

    // update tag
    victim->tag = paddr.ct;
    victim->block[paddr.co] = data;
    return;


}




