#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../header/linker.h"
#include "../header/common.h"

// int read_elf(const char *filename, uint64_t buf);
// int parse_table_entry(char *str, char ***ent);

// void parse_sh(char *str, sh_entry_t *sh);
// void free_table_entry(char **ent, int n);

// void print_sh_entry(char *filename, elf_t *elf);
// void free_elf(elf_t *elf);



int main(){

    // test link_elf
    elf_t src[2];
    parse_elf("./files/exe/sum.elf.txt", &src[0]);
    parse_elf("./files/exe/main.elf.txt", &src[1]);
    

    elf_t dst;
    elf_t *srcp[2];
    srcp[0] = &src[0];
    srcp[1] = &src[1];
    link_elf((elf_t **)&srcp, 2, &dst);


    write_eof("./files/exe/output.eof.txt", &dst);
    free_elf(&src[0]);
    free_elf(&src[1]);
    
    return 0;
   


    // test parse_elf
    // elf_t src[2];
    // parse_elf("./files/exe/sum.elf.txt", &src[0]);
    // parse_elf("./files/exe/main.elf.txt", &src[1]);
    
    // free_elf(&src[0]);
    // free_elf(&src[1]);
    // return 0;
    
    
    
    
    // test read_elf
    // char buf[MAX_ELF_FILE_LENGTH][MAX_ELF_FILE_WIDTH];
    // int count = read_elf("./files/exe/sum.elf.txt", (uint64_t)&buf);

    // for (int i = 0; i < count; i++){
    //     printf("%s\n", buf[i]);
    // }

    // return 0;
}