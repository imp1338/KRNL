#include <ntddk.h>
#include "event_mgr.h"
#include "../common/definitions.h"
#include "../common/logging.h"

VOID event_manager_initialize(p_validation_context context) {
    if (!context) {
        log_error_simple("invalid validation context");
        return;
    }

    log_info_simple("initializing validation event...");

    KeInitializeEvent(&context->validation_event, NotificationEvent, FALSE);
    context->key_validated = FALSE;
    context->timed_out = FALSE;

    log_success_simple("validation event initialized");
}

NTSTATUS event_manager_wait_for_validation(p_validation_context context, PLARGE_INTEGER timeout) {
    NTSTATUS status;

    if (!context) {
        log_error_simple("invalid validation context");
        return STATUS_INVALID_PARAMETER;
    }

    log_info("waiting for validation (timeout: %d seconds)...", validation_timeout_seconds);

    status = KeWaitForSingleObject(&context->validation_event,Executive,KernelMode,FALSE,timeout);

    if (status == STATUS_TIMEOUT) {
        log_warning("validation timeout - no response received");
        context->timed_out = TRUE;
        return status_key_validation_timeout;
    }

    if (!NT_SUCCESS(status)) {
        log_error("wait failed with status: 0x%X", status);
        return status;
    }

    if (!context->key_validated) {
        log_error_simple("key validation failed - invalid key entered");
        return status_key_validation_failed;
    }

    log_success_simple("key validated successfully!");
    return STATUS_SUCCESS;
}

VOID event_manager_set_validation(p_validation_context context, BOOLEAN is_valid) {
    if (!context) {
        log_error_simple("invalid validation context");
        return;
    }

    log_info("setting validation result: %s", is_valid ? "valid" : "invalid");

    context->key_validated = is_valid;
    KeSetEvent(&context->validation_event, IO_NO_INCREMENT, FALSE);

    log_info_simple("validation event signaled");
}

VOID event_manager_cleanup(p_validation_context context) {
    if (!context) {
        return;
    }

    log_info_simple("cleaning up validation context...");

    context->key_validated = FALSE;
    context->timed_out = FALSE;

    log_success_simple("validation context cleaned up");
}