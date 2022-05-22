// DRAM : Dynamic Random Access Memory
#include <string.h>
#include <assert.h>
#include "../../header/cpu.h"
#include "../../header/memory.h"
#include "../../header/common.h"
#include "../../header/address.h"

uint8_t sram_cache_read(uint64_t paddr);
void sram_cache_write(uint64_t paddr, uint8_t data);

// #define SRAM_CACHE_SETTING 0  //  开关cashe功能，cache功能以后写


/*
    Be careful with the x86 little endian integer encoding
    e.g. write 0x00007fd357a02ae0 to cache, the memory lapping should be:
    e0 2a a0 57 d3 7f 00 00
*/

// memory accessing used in instruction
uint64_t cpu_read64bits_dram(uint64_t paddr){

    uint64_t val = 0x0;
#ifdef USE_SRAM_CACHE
    
    //try to load uint64_t from SRAM cache
    // little-endian
    
    
    for (int i = 0; i < 8; ++i){
        val += (sram_cache_read(paddr + i) << (i * 8));
    }
    
#else
        
    // read from DRAM directly
    // little-endian

    val += (((uint64_t)pm[paddr + 0]) << 0);
    val += (((uint64_t)pm[paddr + 1]) << 8);
    val += (((uint64_t)pm[paddr + 2]) << 16);
    val += (((uint64_t)pm[paddr + 3]) << 24);
    val += (((uint64_t)pm[paddr + 4]) << 32);
    val += (((uint64_t)pm[paddr + 5]) << 40);
    val += (((uint64_t)pm[paddr + 6]) << 48);
    val += (((uint64_t)pm[paddr + 7]) << 56);
#endif
    
    return val;


}


void cpu_write64bits_dram(uint64_t paddr, uint64_t data){

#ifdef USE_SRAM_CACHE
        
    // try to write uint64_t to SRAM cache
    // little-endian
    for (int i = 0; i < 8; ++i){
        sram_cache_write(paddr + i, (data >> (i * 8)) & 0xff);
    }
    return;

    
#else
    // write to DRAM diretly
    // little-endian
    pm[paddr + 0] = (data >> 0) & 0xff;
    pm[paddr + 1] = (data >> 8) & 0xff;
    pm[paddr + 2] = (data >> 16) & 0xff;
    pm[paddr + 3] = (data >> 24) & 0xff;
    pm[paddr + 4] = (data >> 32) & 0xff;
    pm[paddr + 5] = (data >> 40) & 0xff;
    pm[paddr + 6] = (data >> 48) & 0xff;
    pm[paddr + 7] = (data >> 56) & 0xff;
#endif
    
    
}

void cpu_readinst_dram(uint64_t paddr, char *buf){

    for (int i = 0; i < MAX_INSTRUCTION_CHAR; ++i){
        buf[i] = (char)pm[paddr + i];
    }

}

void cpu_writeinst_dram(uint64_t paddr, const char *str){

    int len = strlen(str);
    assert(len < MAX_INSTRUCTION_CHAR);

    for (int i = 0; i < MAX_INSTRUCTION_CHAR; ++i){
        
        if (i < len){
            pm[paddr + i] = (uint8_t)str[i];
        }
        else {
            pm[paddr + i] = 0;
        }
    }

}


/* interface of I/O Bus: read and write between the SRAM cache and DRAM memory
 */

void bus_read_cacheline(uint64_t paddr, uint8_t *block){


    uint64_t dram_base = ((paddr >> SRAM_CACHE_OFFSET_LENGTH) << SRAM_CACHE_TAG_LENGTH);

    for (int i = 0; i < (1 << SRAM_CACHE_OFFSET_LENGTH); ++i){

        block[i] = pm[dram_base + i];
    }

} 


void bus_write_cacheline(uint64_t paddr, uint8_t *block){

    uint64_t dram_base = ((paddr >> SRAM_CACHE_OFFSET_LENGTH) << SRAM_CACHE_TAG_LENGTH);

    for (int i = 0; i < (1 << SRAM_CACHE_OFFSET_LENGTH); ++i){

        pm[dram_base + i] = block[i];
    }

}



