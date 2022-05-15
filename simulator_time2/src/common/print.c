#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <header/common.h>


#include<stdint.h>

// wrapper of stdio printf
// controlled by the debug verbose bit set
uint64_t debug_printf(uint64_t open_set, const char *format, ...){

    if ((open_set & DEBUG_VERBOSE_SET) == 0x0){
        return 0x1;
    }


    // Implementation of std printf()
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);

    return 0x0;
}