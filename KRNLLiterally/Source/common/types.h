#pragma once

#ifndef TYPES_H
#define TYPES_H

#include <ntddk.h>

typedef struct _validation_context validation_context_t, * p_validation_context;
typedef struct _injection_context injection_context_t, * p_injection_context;

// pictures of last night ended up online im screwed, ow well
typedef struct _validation_context {
    KEVENT validation_event;
    BOOLEAN key_validated;
    BOOLEAN timed_out;
} validation_context_t, * p_validation_context;

typedef struct _injection_context {
    PEPROCESS target_process;
    PVOID allocated_memory;
    SIZE_T allocation_size;
    HANDLE thread_handle;
} injection_context_t, * p_injection_context;

typedef struct _driver_context {
    PDEVICE_OBJECT device_object;
    validation_context_t validation;
} driver_context_t, * p_driver_context;

#endif