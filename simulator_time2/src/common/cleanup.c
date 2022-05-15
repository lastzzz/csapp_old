#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../header/common.h"
#include "../header/algorithm.h"

typedef void (*cleanup_t)();

static array_t *events = NULL;

void add_cleanup_events(void *func){
    assert(events != NULL);

    if (events == NULL){
        
        // uninitialized - lazy malloc
        // start from 8 slots
        events = array_constuct(8);
    }

    // fill in the first event
    array_insert(&events, (uint64_t)func);
    return;
}


void finally_cleanup(){
    for (int i=0; i < events->count; ++i){
        uint64_t address;
        assert(array_get(events, i, &address) != NULL);

        cleanup_t *func;
        *(uint64_t *)&func = address;
        (*func)();
    }

    //clean itself
    array_free(events);
    events = NULL;
}


