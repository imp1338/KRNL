#include <ntifs.h>
#include "shell_inj.h"
#include "../proc/procmgr.h"
#include "../../payload/shellcode.h"
#include "../../common/definitions.h"
#include "../../common/logging.h"
#include "../pe/pe_parser.h"
#include "../stub/inj_stub.h"
#include "../mem/memalloc.h"
#include <minwindef.h>

extern "C" NTSYSAPI NTSTATUS NTAPI RtlCreateUserThread(
    IN HANDLE ProcessHandle,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor OPTIONAL,
    IN BOOLEAN CreateSuspended,
    IN ULONG StackZeroBits OPTIONAL,
    IN OUT PULONG StackReserved OPTIONAL,
    IN OUT PULONG StackCommit OPTIONAL,
    IN PVOID StartAddress,
    IN PVOID StartParameter OPTIONAL,
    OUT PHANDLE ThreadHandle,
    OUT PCLIENT_ID ClientId
);

// https://www.youtube.com/watch?v=jzjIJ56wi-k

NTSTATUS perform_injection(PCSTR target_proc) {
    NTSTATUS status;
    PEPROCESS target_process;
    manual_map_context_t map_context = { 0 };
    PVOID stub_address = NULL;
    PVOID loader_data_remote = NULL;
    HANDLE process_handle = NULL;
    HANDLE thread_handle = NULL;

    if (!target_proc) {
        log_error_simple("invalid target process name");
        return STATUS_INVALID_PARAMETER;
    }
    log_info("starting dll injection into: %s", target_proc);

    target_process = process_manager_find_by_name(target_proc);
    if (!target_process) {
        log_error("failed to find target process: %s", target_proc);
        return STATUS_NOT_FOUND;
    }

    status = ObOpenObjectByPointer(target_process,OBJ_KERNEL_HANDLE,NULL,PROCESS_ALL_ACCESS,*PsProcessType,KernelMode,&process_handle);
    if (!NT_SUCCESS(status)) {
        log_error("failed to get process handle: 0x%X", status);
        return status;
    }

    status = pe_loader_map_dll(target_process,shellcode_get_buffer(),shellcode_get_size(),&map_context);
    if (!NT_SUCCESS(status)) {
        ZwClose(process_handle);
        return status;
    }

    log_success("dll mapped at: 0x%p", map_context.image_base);

    SIZE_T stub_size = loader_stub_get_size();
    status = memory_allocator_allocate(target_process,stub_size,&stub_address);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    status = memory_allocator_write(target_process,stub_address,loader_stub_get_code(),stub_size);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    log_info("loader stub at: 0x%p", stub_address);

    LOADER_DATA loader_data = { 0 };
    loader_data.entry_point = map_context.entry_point;
    loader_data.image_base = map_context.image_base;

    status = memory_allocator_allocate(target_process,sizeof(LOADER_DATA),&loader_data_remote);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    status = memory_allocator_write(target_process,loader_data_remote,&loader_data,sizeof(loader_data));
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    // this friday night, do it alllllll again
    log_info("creating remote thread at %p with loader data %p", stub_address, loader_data_remote);

    CLIENT_ID client_id = { 0 };
    status = RtlCreateUserThread(process_handle, NULL, FALSE, 0, 0, 0, stub_address, loader_data_remote, &thread_handle, &client_id);
    if (!NT_SUCCESS(status)) {
        goto cleanup;
    }

    log_success("remote thread created (TID: %u)", client_id.UniqueThread);

    THREAD_BASIC_INFORMATION info = { 0 };
    ULONG ret_len;
    ZwQueryInformationThread(thread_handle, ThreadBasicInformation, &info, sizeof(info), &ret_len);
    log_info("thread exit status: 0x%X", info.ExitStatus);

    ULONGLONG value = 0;
    SIZE_T bytes = 0;

	ObDereferenceObject(target_process);
    
    ZwClose(thread_handle);
    ZwClose(process_handle);
    return STATUS_SUCCESS;

cleanup:
    if (loader_data_remote)
        memory_allocator_free(target_process, loader_data_remote);

    if (stub_address)
        memory_allocator_free(target_process, stub_address);

    pe_loader_cleanup(target_process, &map_context);
    ZwClose(process_handle);

    return status;
}


VOID injector_cleanup(p_injection_context context) {
    if (!context) {
        return;
    }

    log_info_simple("cleaning up injection context...");

    if (context->allocated_memory && context->target_process) {
        memory_allocator_free(context->target_process, context->allocated_memory);
    }

    if (context->thread_handle) {
        ZwClose(context->thread_handle);
    }

    RtlZeroMemory(context, sizeof(injection_context_t));

    log_success_simple("injection context cleaned up");
}