#include"cpu/register.h"
#include"cpu/mmu.h"
#include"memory/instruction.h"
#include <stdio.h>
#include "memory/dram.h"
#include "disk/elf.h"


int main (){
    
    init_handler_table();

    reg.rax = 0x12340000;
    reg.rbx = 0x0;
    reg.rcx = 0x8000660;
    reg.rdx = 0xabcd;
    reg.rsi = 0x7ffffffee2f8;
    reg.rdi = 0x1;
    reg.rbp = 0x7ffffffee210;
    reg.rsp = 0x7ffffffee1f0;
    reg.rip = (uint64_t)&program[11];


    write64bits_dram(va2pa(0x7ffffffee210), 0x08000660);//rbp
    write64bits_dram(va2pa(0x7ffffffee208), 0x0);
    write64bits_dram(va2pa(0x7ffffffee200), 0xabcd);
    write64bits_dram(va2pa(0x7ffffffee1f8), 0x12340000);
    write64bits_dram(va2pa(0x7ffffffee1f0), 0x08000660);//rsp

    print_register();
    print_stack();
   
    // 验证write函数
    // uint64_t pa = va2pa(0x7ffffffee210);
    // printf("%16lx\n", *((uint64_t *) (&mm[pa])));

    
    // 验证read函数
    // printf("%lx\n", read64bits_dram(va2pa(0x7ffffffee210)));

    // mm[va2pa(0x7ffffffee210)] = 0x08000660;  //rbp
    // mm[va2pa(0x7ffffffee208)] = 0x0;
    // mm[va2pa(0x7ffffffee200)] = 0xabcd;
    // mm[va2pa(0x7ffffffee1f8)] = 0x12340000;
    // mm[va2pa(0x7ffffffee1f0)] = 0x08000660;  //rsp

    for (int i = 0; i < 15; i++){
        instruction_cycle();
        print_register();
        print_stack();
    }


    int match = 1;

    match = match && (reg.rax == 0x1234abcd);
    match = match && (reg.rbx == 0x0);
    match = match && (reg.rcx == 0x8000660);
    match = match && (reg.rdx == 0x12340000);
    match = match && (reg.rsi == 0xabcd);
    match = match && (reg.rdi == 0x12340000);
    match = match && (reg.rbp == 0x7ffffffee210);
    match = match && (reg.rsp == 0x7ffffffee1f0);

    if (match == 1){
        printf("register match\n");
    }
    else{
        printf("register not match\n");
    }

    match = 1;

    match = match && (read64bits_dram(va2pa(0x7ffffffee210)) == 0x08000660);
    match = match && (read64bits_dram(va2pa(0x7ffffffee208)) == 0x1234abcd);
    match = match && (read64bits_dram(va2pa(0x7ffffffee200)) == 0xabcd);
    match = match && (read64bits_dram(va2pa(0x7ffffffee1f8)) == 0x12340000);
    match = match && (read64bits_dram(va2pa(0x7ffffffee1f0)) == 0x08000660);

    if (match == 1){
        printf("memory match\n");
    }
    else {
        printf("memory not match\n");
    }




    return 0;
}

