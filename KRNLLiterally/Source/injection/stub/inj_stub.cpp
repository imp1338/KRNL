#include "inj_stub.h"

// row taper frade hehehehe
// IM ON FIREEEE IM ON FIREEEEE IM ON FIREEEEEEEEEE IM ON FIREEEE, FIREBALL

unsigned char g_loader_stub[] = {
    0x48, 0x83, 0xEC, 0x28,             // sub rsp, 0x28
    0x48, 0x8B, 0x01,                   // mov rax, [rcx]
    0x48, 0x8B, 0x49, 0x08,             // mov rcx, [rcx+8]
    0xBA, 0x01, 0x00, 0x00, 0x00,       // mov edx, 1
    0x4D, 0x33, 0xC0,                   // xor r8, r8
    0xFF, 0xD0,                         // call rax
    0x48, 0x83, 0xC4, 0x28,             // add rsp, 0x28
    0xC3                                // ret
};

PVOID loader_stub_get_code() {
    return g_loader_stub;
}

SIZE_T loader_stub_get_size() {
    return sizeof(g_loader_stub);
}