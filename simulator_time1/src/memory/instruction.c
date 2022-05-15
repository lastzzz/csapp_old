#include "instruction.h"
#include "../cpu/mmu.h"
#include "../cpu/register.h"
#include <stdio.h>
#include "dram.h"

static uint64_t decode_od(od_t od){

    
    if (od.type == IMM){
        return *((uint64_t *) &od.imm);//返回imm的值
    }
    else if (od.type == REG){
        return (uint64_t) od.reg1;// 返回指向reg的地址
    }
    else {
        uint64_t vaddr = 0;
        if (od.type == MM_IMM){
            vaddr = od.imm;
        }
        else if (od.type == MM_REG){
            vaddr = *(od.reg1);
        }
        else if (od.type == MM_IMM_REG){
            vaddr = od.imm + *(od.reg1);
        }
        else if (od.type == MM_REG1_REG2){
            vaddr = *(od.reg1) + *(od.reg2);
        }
        else if (od.type == MM_IMM_REG1_REG2){
            vaddr = *(od.reg1) + *(od.reg2) + od.imm;
        }
        else if (od.type == MM_REG2_S){
            vaddr = (*(od.reg2)) * od.scal;
        }
        else if (od.type == MM_IMM_REG2_S){
            vaddr = od.imm + (*(od.reg2)) * od.scal;
        }
        else if (od.type == MM_REG1_REG2_S){
            vaddr = *(od.reg1) + (*(od.reg2)) * od.scal;
        }
        else if (od.type == MM_IMM_REG1_REG2_S){
            vaddr = od.imm + *(od.reg1) + (*(od.reg2)) * od.scal;
        }
        return vaddr;

    }

}


void init_handler_table(){
    
    handler_table[mov_reg_reg] = &mov_reg_reg_handler;
    handler_table[add_reg_reg] = &add_reg_reg_handler;
    handler_table[call] = &call_handler;
    handler_table[push_reg] = &push_reg_handler;
    handler_table[mov_reg_mem] = &mov_reg_mem_handler;
    handler_table[mov_mem_reg] = &mov_mem_reg_handler;
    handler_table[ret] = &ret_handler;
    handler_table[pop_reg] = &pop_reg_handler;
}

void instruction_cycle(){

    inst_t *instr = (inst_t *)reg.rip;

    uint64_t src = decode_od(instr->src);
    uint64_t dst = decode_od(instr->dst);

    handler_t handler = handler_table[instr->op];
    handler(src, dst);
    printf("=======================================================>    %s\n", instr->code);

}

void mov_reg_reg_handler(uint64_t src, uint64_t dst){

    // src: reg
    // dst: reg
    *(uint64_t *)dst =  *(uint64_t *)src;//mov操作
    reg.rip = reg.rip + sizeof(inst_t);//更新cp

}

void mov_reg_mem_handler(uint64_t src, uint64_t dst){

    //src: reg
    //dst: mem virutal address
    //mov操作
    write64bits_dram(va2pa(dst), *(uint64_t *)src);
    reg.rip = reg.rip + sizeof(inst_t);//更新cp

}

void mov_mem_reg_handler(uint64_t src, uint64_t dst){

    // src: mem virutal address
    // dst: reg
    *(uint64_t *)dst = read64bits_dram(va2pa(src));
    
    reg.rip += sizeof(inst_t);

}

void add_reg_reg_handler(uint64_t src, uint64_t dst){
    //src: reg
    //dst: reg

    *(uint64_t *)dst =  *(uint64_t *)dst + *(uint64_t *)src; //add操作
    reg.rip = reg.rip + sizeof(inst_t);// cp指向下一条指令
}

void push_reg_handler(uint64_t src, uint64_t dst){

    // src: reg
    // dst: empty

    reg.rsp -= 0x8;  // stack 下移8个字节
    write64bits_dram(va2pa(reg.rsp), *(uint64_t *)src);//将reg指向的memory内容压入栈
    reg.rip += sizeof(inst_t); //cp 指向下一条指令
}

void pop_reg_handler(uint64_t src, uint64_t dst){
    
    //src: rbp:reg
    //dst: empty
    //***********************************************************感觉是dst为rbp，dst为空，但是没改
    *(uint64_t *)src = read64bits_dram(va2pa(reg.rsp)); //将当前rsp值传给src，也就是rbp
    reg.rsp += 8;   //将rsp上移
    reg.rip += sizeof(inst_t);//指向下一个指令

}

void ret_handler(uint64_t src, uint64_t dst){

    //src: empty
    //dst: empty
    uint64_t ret_addr = read64bits_dram(va2pa(reg.rsp));//将返回地址传给ret_addr
    reg.rsp += 8;
    reg.rip = ret_addr;//将cp值指向ret_addr

}

void call_handler(uint64_t src, uint64_t dst){

    //src: imm address of called function
    //dst:  
    reg.rsp -= 0x8; // stack 下移8个字节
    //write return address to rsp memory
    write64bits_dram(va2pa(reg.rsp), reg.rip + sizeof(inst_t)); //将cp+1指向的指令地址压入栈中

    reg.rip = src;// 改变cp的值为所call函数的地址
}
