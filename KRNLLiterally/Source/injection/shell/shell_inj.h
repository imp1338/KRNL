#pragma once

#ifndef SHELL_INJ_H
#define SHELL_INJ_H

#include <ntddk.h>
#include "../../common/types.h"
#include "../../common/definitions.h"

typedef struct _THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    LONG Priority;
    LONG BasePriority;
} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;

extern "C" NTKERNELAPI NTSTATUS NTAPI ZwQueryInformationThread(
    HANDLE ThreadHandle,
    THREADINFOCLASS ThreadInformationClass,
    PVOID ThreadInformation,
    ULONG ThreadInformationLength,
    PULONG ReturnLength
);

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

NTSTATUS perform_injection(PCSTR target_proc);
VOID apc_injection_callback(PVOID NormalContext, PVOID SystemArgument1, PVOID SystemArgument2);
VOID injector_cleanup(p_injection_context context);

#endif