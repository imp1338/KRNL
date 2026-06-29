#include <ntddk.h>
#include "driver.h"
#include "../communication/device.h"
#include "../communication/ioctl.h"
#include "../validation/key_validator.h"
#include "../injection/shell/shell_inj.h"
#include "../common/definitions.h"
#include "../common/logging.h"

static driver_context_t g_driver_context = { 0 };

p_driver_context driver_get_context() {
    return &g_driver_context;
}

VOID driver_set_context(p_driver_context context) {
    if (context) {
        RtlCopyMemory(&g_driver_context, context, sizeof(driver_context_t));
    }
}

NTSTATUS driver_initialize(PDRIVER_OBJECT driver_object) {
    NTSTATUS status;
    p_driver_context context = &g_driver_context;
    RtlZeroMemory(context, sizeof(driver_context_t));

    status = device_create(driver_object, &context->device_object);
    if (!NT_SUCCESS(status)) {
        log_error("failed to create device: 0x%X", status);
        return status;
    }

    status = device_create_symlink();
    if (!NT_SUCCESS(status)) {
        log_error("failed to create symbolic link: 0x%X", status);
        device_cleanup(context->device_object);
        context->device_object = NULL;
        return status;
    }

    status = validator_initialize(&context->validation);
    if (!NT_SUCCESS(status)) {
        log_error("failed to initialize validator: 0x%X", status);
        device_cleanup(context->device_object);
        return status;
    }

    ioctl_set_validation_context(&context->validation);

    log_success_simple("driver initialization completed");
    return STATUS_SUCCESS;
}

VOID MakeBeep(ULONG frequency, ULONG duration_ms)
{
    ULONG divisor = 1193180 / frequency;

    WRITE_PORT_UCHAR((PUCHAR)0x43, 0xB6);
    WRITE_PORT_UCHAR((PUCHAR)0x42, (UCHAR)(divisor & 0xFF));
    WRITE_PORT_UCHAR((PUCHAR)0x42, (UCHAR)(divisor >> 8));

    UCHAR tmp = READ_PORT_UCHAR((PUCHAR)0x61);
    WRITE_PORT_UCHAR((PUCHAR)0x61, tmp | 0x03);

    LARGE_INTEGER delay;
    delay.QuadPart = -(LONGLONG)(duration_ms * 10000);
    KeDelayExecutionThread(KernelMode, FALSE, &delay);

    // Disable speaker
    WRITE_PORT_UCHAR((PUCHAR)0x61, tmp);
}

PEPROCESS process_manager_find_by_name(const char* process_name) {
    PEPROCESS current_process = NULL;
    PEPROCESS system_process = NULL;
    PLIST_ENTRY list_head = NULL;
    PLIST_ENTRY list_entry = NULL;

    if (!process_name) {
        log_error_simple("invalid process name");
        return NULL;
    }

    log_info("searching for process: %s", process_name);

    system_process = PsGetCurrentProcess();
    list_head = (PLIST_ENTRY)((PUCHAR)system_process + activeprocesslinks_offset);
    list_entry = list_head->Flink;

    while (list_entry != list_head) {
        current_process = (PEPROCESS)((PUCHAR)list_entry - activeprocesslinks_offset);
        char* image_name = (char*)((PUCHAR)current_process + imagefilename_offset);

        if (_stricmp(image_name, process_name) == 0) {
            ObReferenceObject(current_process);

            // Check if process is actually running
            if (PsGetProcessExitStatus(current_process) != STATUS_PENDING) {
                ObDereferenceObject(current_process);
                break;  // Stop searching - found a zombie
            }

            log_success("found process: %s at 0x%p", process_name, current_process);
            return current_process;
        }

        list_entry = list_entry->Flink;
    }

    log_error("process not found or terminating: %s", process_name);
    return NULL;
}

NTSTATUS AwaitProcess(p_driver_context context)
{
    PEPROCESS target_process = NULL;
    LARGE_INTEGER interval, delay;
    interval.QuadPart = -1000000LL;

    log_info("waiting for process: %s", target_process_name);
    
    while (TRUE) {

        for (int i = 0; i < 2; i++) {
            delay.QuadPart = -10000000LL; // 1 second
            KeDelayExecutionThread(KernelMode, FALSE, &delay);
        }

        target_process = process_manager_find_by_name(target_process_name);

        if (target_process) {
            ObDereferenceObject(target_process);

			MakeBeep(800, 600);

            for (int i = 0; i < 10; i++) {
                MakeBeep(1000, 150);
                delay.QuadPart = -10000000LL; // 1 second
                KeDelayExecutionThread(KernelMode, FALSE, &delay);
            }

            // Final confirmation beep
            MakeBeep(200, 1300);
            log_success("process found: %s", target_process_name);
            return STATUS_SUCCESS;
        }
    }
}
NTSTATUS driver_start(p_driver_context context) {
    NTSTATUS status;

    if (!context) {
        log_error_simple("invalid driver context");
        return STATUS_INVALID_PARAMETER;
    }

    status = AwaitProcess(context);
    if (!NT_SUCCESS(status)) {
        log_error("process wait failed or cancelled: 0x%X", status);
        return status;
    }

    log_info_simple("attempting injection...");


    status = perform_injection(target_process_name);
    if (!NT_SUCCESS(status)) {
        log_error("injection failed: 0x%X", status);
    }
    else {
        log_success_simple("injection completed");
    }

    return STATUS_SUCCESS;
}

VOID driver_cleanup(PDRIVER_OBJECT driver_object, p_driver_context context) {
    UNREFERENCED_PARAMETER(driver_object);
    if (!context) {
        log_warning("no context to clean up");
        return;
    }

    validator_cleanup(&context->validation);

    if (context->device_object) {
        device_cleanup(context->device_object);
        context->device_object = NULL;
    }

    RtlZeroMemory(context, sizeof(driver_context_t));
}