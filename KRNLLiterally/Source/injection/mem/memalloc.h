#pragma once

#ifndef MEMALLOC_H
#define MEMALLOC_H

#include <ntddk.h>

extern "C" NTKERNELAPI PPEB NTAPI PsGetProcessPeb(
    PEPROCESS Process
);

NTSTATUS memory_allocator_allocate(
    PEPROCESS target_process,
    SIZE_T size,
    PVOID* allocated_memory
);

NTSTATUS memory_allocator_write(
    PEPROCESS target_process,
    PVOID destination_address,
    PVOID source_buffer,
    SIZE_T size
);

NTSTATUS memory_allocator_free(
    PEPROCESS target_process,
    PVOID memory
);

#endif