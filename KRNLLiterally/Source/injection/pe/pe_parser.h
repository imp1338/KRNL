#pragma once

#ifndef PE_LOADER_H
#define PE_LOADER_H

#include <ntddk.h>

typedef struct _IMAGE_THUNK_DATA64 {
    union {
        ULONGLONG ForwarderString;  // PUCHAR - pointer to a forwarder string
        ULONGLONG Function;         // PUCHAR - pointer to the function
        ULONGLONG Ordinal;          // Ordinal value
        ULONGLONG AddressOfData;    // PIMAGE_IMPORT_BY_NAME - pointer to data
    } u1;
} IMAGE_THUNK_DATA64, *PIMAGE_THUNK_DATA64;

// 64 bit ordial add 32b to support 32bit injection
#define IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL

#define IMAGE_SNAP_BY_ORDINAL64(Ordinal) \
    (((Ordinal) & IMAGE_ORDINAL_FLAG64) != 0)

#define IMAGE_ORDINAL64(Ordinal) \
    ((Ordinal) & 0xFFFF)



typedef IMAGE_THUNK_DATA64 IMAGE_THUNK_DATA;
typedef PIMAGE_THUNK_DATA64 PIMAGE_THUNK_DATA;
#define IMAGE_ORDINAL_FLAG IMAGE_ORDINAL_FLAG64
#define IMAGE_SNAP_BY_ORDINAL(Ordinal) IMAGE_SNAP_BY_ORDINAL64(Ordinal)
#define IMAGE_ORDINAL(Ordinal) IMAGE_ORDINAL64(Ordinal)

typedef struct _manual_map_context {
    PVOID image_base;
    SIZE_T image_size;
    PVOID entry_point;
} manual_map_context_t, * p_manual_map_context;

extern "C"
NTKERNELAPI
NTSTATUS
MmCopyVirtualMemory(
    PEPROCESS FromProcess,
    PVOID FromAddress,
    PEPROCESS ToProcess,
    PVOID ToAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T NumberOfBytesCopied
);

// yeah i think we broke the law always say were gonna stop oh oh ohhh

NTSTATUS pe_loader_map_dll(
    PEPROCESS target_process,
    PVOID dll_buffer,
    SIZE_T dll_size,
    p_manual_map_context map_context
);

VOID pe_loader_cleanup(
    PEPROCESS target_process,
    p_manual_map_context map_context
);

#endif