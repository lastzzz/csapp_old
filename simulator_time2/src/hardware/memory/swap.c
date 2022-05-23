#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../header/cpu.h"
#include "../../header/memory.h"
#include "../../header/common.h"
#include "../../header/address.h"


// each swap file is swap page
// each line of this swap page is one uint64
#define SWAP_PAGE_FILE_LINES (512)

// disk address counter
// static uint64_t internal_swap_daddr = 0;


int swap_in(uint64_t daddr, uint64_t ppn){

    FILE *fr = NULL;
    char filename[128];
    sprintf(filename, "../files/swap/page-%ld.txt", daddr);
    fr = fopen(filename, "r");
    assert(fr != NULL);

    uint64_t ppn_ppo = ppn << PHYSICAL_PAGE_NUMBER_LENGTH;
    char buf[64] = {'0'};
    for (int i = 0; i < SWAP_PAGE_FILE_LINES; ++i){
        // 每次从swap文件读一行（一行为8字节），将一行转化为二进制写入内存
        char *str = fgets(buf, 64, fr);
        *((uint64_t *)(&pm[ppn_ppo + i * 8])) = string2uint(str);
    }
    fclose(fr);
    return 0;
}


int swap_out(uint64_t daddr, uint64_t ppn){
    
    FILE *fw = NULL;
    char filename[128];
    sprintf(filename, "../files/swap/page-%ld.txt", daddr);
    fw = fopen(filename, "w");
    assert(fw != NULL);

    uint64_t ppn_ppo = ppn << PHYSICAL_PAGE_NUMBER_LENGTH;
    for (int i = 0; i < SWAP_PAGE_FILE_LINES; ++i){
        // 每次写8个字节,也就是说swap文件每一行写64位，就是8个字节
        fprintf(fw, "0x16%lx\n", *((uint64_t *)(&pm[ppn_ppo + i * 8])));

    }
    fclose(fw);
    return 0;

}

