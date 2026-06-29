#include <ntifs.h>
#include <ntddk.h>
#include "memalloc.h"
#include "../../common/logging.h"

NTSTATUS memory_allocator_allocate(PEPROCESS target_process,SIZE_T size,PVOID* allocated_memory) {
    NTSTATUS status;
    KAPC_STATE apc_state;
    PVOID memory = NULL;
    SIZE_T region_size = size;

    if (!target_process || !allocated_memory || size == 0) {
        log_error_simple("invalid parameters for memory allocation");
        return STATUS_INVALID_PARAMETER;
    }

    log_info("allocating %zu bytes in target process...", size);
    KeStackAttachProcess(target_process, &apc_state);

    __try {
        status = ZwAllocateVirtualMemory(ZwCurrentProcess(),&memory,0,&region_size,MEM_COMMIT | MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        if (!NT_SUCCESS(status)) {
            log_error("failed to allocate memory: 0x%X", status);
            KeUnstackDetachProcess(&apc_state);
            return status;
        }
        log_success("allocated memory at: 0x%p (size: %zu bytes)", memory, region_size);
        *allocated_memory = memory;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        log_error("exception during memory allocation: 0x%X", status);
    }

    KeUnstackDetachProcess(&apc_state);

    return status;
}

NTSTATUS memory_allocator_write(PEPROCESS target_process,PVOID destination_address,PVOID source_buffer,SIZE_T size) {
    NTSTATUS status = STATUS_SUCCESS;
    KAPC_STATE apc_state;

    if (!target_process || !destination_address || !source_buffer || size == 0) {
        log_error_simple("invalid parameters for memory write");
        return STATUS_INVALID_PARAMETER;
    }
    // LAST FRIDAY NIGHT, YEAH WE DANCED ON TABLETOPS
    log_info("writing %zu bytes to 0x%p...", size, destination_address);
    KeStackAttachProcess(target_process, &apc_state);
    __try {
        RtlCopyMemory(destination_address, source_buffer, size);
        log_success_simple("memory write completed successfully");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        log_error("exception during memory write: 0x%X", status);
    }

    KeUnstackDetachProcess(&apc_state);

    return status;
}

NTSTATUS memory_allocator_read(PEPROCESS target_process,PVOID source,PVOID destination,SIZE_T size)
{
    if (!target_process || !source || !destination || size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status = STATUS_SUCCESS;
    KAPC_STATE apc_state;
    SIZE_T bytes_read = 0;

    __try {

        log_info("reading %zu bytes from 0x%p...", size, destination);
        KeStackAttachProcess(target_process, &apc_state);
        if (!MmIsAddressValid(source)) {
            status = STATUS_INVALID_ADDRESS;
            KeUnstackDetachProcess(&apc_state);
            return status;
        }

        RtlCopyMemory(destination, source, size);
        bytes_read = size;
        KeUnstackDetachProcess(&apc_state);
        status = STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (apc_state.Process != NULL) {
            KeUnstackDetachProcess(&apc_state);
        }
        status = GetExceptionCode();
    }

    return status;
}

NTSTATUS memory_allocator_free(PEPROCESS target_process,PVOID memory) {
    NTSTATUS status;
    KAPC_STATE apc_state;
    SIZE_T region_size = 0;

    if (!target_process || !memory) {
        log_error_simple("invalid parameters for memory free");
        return STATUS_INVALID_PARAMETER;
    }

    log_info("freeing memory at: 0x%p", memory);

    KeStackAttachProcess(target_process, &apc_state);

    __try {
        status = ZwFreeVirtualMemory(ZwCurrentProcess(),&memory,&region_size,MEM_RELEASE);
        if (!NT_SUCCESS(status)) {
            log_error("failed to free memory: 0x%X", status);
        }
        else {
            log_success_simple("memory freed successfully");
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        status = GetExceptionCode();
        log_error("exception during memory free: 0x%X", status);
    }
    // YEAH WE MAXED OUR CREDIT CARDS AND GOT KICKED OUT OF THE BAR
    KeUnstackDetachProcess(&apc_state);
    // best day of the year girl, BIRTHDAY SEX, BIRTHDAY SEX
    return status;
}