#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../../header/cpu.h"
#include "../../header/memory.h"
#include "../../header/common.h"
#include "../../header/address.h"

uint64_t va2pa(uint64_t vaddr){

#ifdef USE_NAVIE_VA2PA
    return vaddr % PHYSICAL_MEMORY_SPACE;
    // return vaddr & (0xffffffffffffffff >> (64 - MAX_NUM_PHYSICAL_PAGE));

    //等价于 vaddr & 0xfffff  <=> vaddr % 65536
#endif

    
    
    
    
    uint64_t paddr = 0;






#if defined(USE_TLB_HARDWARE) && defined(USE_PAGETABLE_VA2PA)
    int free_tlb_line_index = -1;
    int tlb_hit = read_tlb(vaddr, &paddr, &free_tlb_line_index);

    // TODO: add flag to read tlb failed
    if (tlb_hit){
        // TLB read hit
        return paddr;
    }

    // TLB read miss
#endif







#ifdef USE_PAGETABLE_VA2PA
    // assume that page_walk is consuming much time
    paddr = page_walk(vaddr);
#endif






#if defined(USE_TLB_HARDWARE) && defined(USE_PAGETABLE_VA2PA)
    // refresh TLB
    // TODO: check if this paddr from page table is a legal address
    if (paddr != 0){
        // TLB write
        if (write_tlb(vaddr, paddr, free_tlb_line_index) == 1){
            return paddr;
        }
    }
#endif



    // use page table as va2pa
    return paddr;

}






// input - virtual address
// output - physical address
 static uint64_t page_walk(uint64_t vaddr_value);


// input - virtual address
// output - physical address
 static uint64_t page_walk(uint64_t vaddr_value)
{
    // parse address
    address_t vaddr = {
        .vaddr_value = vaddr_value
    };
    int vpns[4] = {
        vaddr.vpn1,
        vaddr.vpn2,
        vaddr.vpn3,
        vaddr.vpn4,
    };
    int vpo = vaddr.vpo;

    int page_table_size = PAGE_TABLE_ENTRY_NUM * sizeof(pte123_t);

    // CR3 register's value is malloced on the heap of the simulator
    pte123_t *pgd = (pte123_t *)cpu_controls.cr3;
    assert(pgd != NULL);
    assert(sizeof(pte123_t) == sizeof(pte4_t));
    assert(page_table_size == (1 << 12));

    int level = 0;
    pte123_t *tab = pgd;
    while (level < 3)
    {
        int vpn = vpns[level];
        if (tab[vpn].present != 1)
        {
            // page fault
            printf("\033[31;1mMMU (%lx): level %d page fault: [%x].present == 0\n\033[0m", vaddr_value, level + 1, vpn);
            goto RAISE_PAGE_FAULT;
        }

        // move to next level
        tab = (pte123_t *)((uint64_t)tab[vpn].paddr);
        level += 1;
    }

    pte4_t *pte = &((pte4_t *)tab)[vaddr.vpn4];
    if (pte->present == 1)
    {
        // find page table entry
        address_t paddr = {
            .ppn = pte->ppn,
            .ppo = vpo    // page offset inside the 4KB page
        };
        return paddr.paddr_value;
    }
    else
    {
        printf("\033[31;1mMMU (%lx): level 4 page fault: [%x].present == 0\n\033[0m", vaddr_value, vaddr.vpn4);
    }

RAISE_PAGE_FAULT:
    mmu_vaddr_pagefault = vaddr.vaddr_value;
    // This interrupt will not return
    interrupt_stack_switching(0x0e);
    return 0;
}