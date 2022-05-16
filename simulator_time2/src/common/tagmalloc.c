#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../header/common.h"
#include "../header/algorithm.h"

static uint64_t compute_tag(char *str);
static void tag_destroy();

typedef struct{
    uint64_t tag;
    uint64_t size;
    void *ptr;
}tag_block_t;


static linkedlist_t *tag_list = NULL;

void *tag_malloc(uint64_t size, char *tagstr){

    uint64_t tag = compute_tag(tagstr);

    tag_block_t *b = malloc(sizeof(tag_block_t));
    b->tag = tag;
    b->size = size;
    b->ptr = malloc(size);

    // manage this block
    if (tag_list == NULL){
        tag_list = linkedlist_construct();
        // remember to clean the tag_list finally
        add_cleanup_events(&tag_destroy);

    }

    // add the heap address to the managin list
    linkedlist_add(tag_list, (uint64_t)b);

    return b->ptr;
}



int tag_free(void *ptr){

    int found = 0;
    // it's very slow because we are managing it 
    for (int i = 0; i < tag_list->count; ++ i){
        linkedlist_node_t *p = linkedlist_next(tag_list);

        tag_block_t *b = (tag_block_t *)p->value;

        if (b->ptr == ptr){
            // found this block
            linkedlist_delete(tag_list, p);
            //free block
            free(b);
            found = 1;
            break;
        }
    }

    if (found == 0){
        return 0;
        // Or we should exit the process at once?
    }

    free(ptr);
    return 1;
}


void tag_sweep(char *tagstr){
    // sweep all the memory with target tag
    // note that this is very dangerous since it will free all tag memory
    // call this function only if you are very confident

    uint64_t tag = compute_tag(tagstr);

    for (int i = 0; i < tag_list->count; ++i){

        linkedlist_node_t *p = linkedlist_next(tag_list);

        tag_block_t *b = (tag_block_t *)p->value;
        if (b->tag == tag){
            // free heap memory
            free(b->ptr);
            // free block
            free(b);
            //free from the linked list
            linkedlist_delete(tag_list, p);
        }
    }
}


static void tag_destroy(){

    for (int i = 0; i < tag_list->count; ++ i){
        linkedlist_node_t *p = linkedlist_next(tag_list);
        tag_block_t *b = (tag_block_t *)p->value;

        //free heap memory
        free(b->ptr);
        // free block
        free(b);
        //free from the linked list
        linkedlist_delete(tag_list, p);
    }
}

// just copy it from hashtable.c
static uint64_t compute_tag(char *str){

    int p = 31;
    int m = 1000000007;

    int k = 0;
    int v = 0;
    for (int i = 0; i < strlen(str); ++i){
        
        v = (v + ((int)str[i] * k) % m) % m;
        k = (k * p) % m;
    }
    return v;
}