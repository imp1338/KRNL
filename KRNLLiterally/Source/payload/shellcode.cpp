#include <ntddk.h>
#include "shellcode.h"

// g_shellcode

unsigned char g_shellcode[18432] = {
    // open krnlhook in hxd and copy as c
    // then paste here
};

// i saw i conquered i came.

SIZE_T g_shellcode_size = sizeof(g_shellcode);

PVOID shellcode_get_buffer() {
    return (PVOID)g_shellcode;
}

SIZE_T shellcode_get_size() {
    return g_shellcode_size;
}