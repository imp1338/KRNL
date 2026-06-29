#include <ntddk.h>
#include "key_validator.h"
#include "event_mgr.h"
#include "../common/definitions.h"
#include "../common/logging.h"

NTSTATUS validator_initialize(p_validation_context context) {
    if (!context) {
        log_error_simple("invalid validation context");
        return STATUS_INVALID_PARAMETER;
    }

    log_info_simple("initializing key validator...");

    event_manager_initialize(context);

    log_success_simple("key validator initialized");
    return STATUS_SUCCESS;
}

NTSTATUS validator_perform_validation(p_validation_context context) {
    NTSTATUS status;
    LARGE_INTEGER timeout;

    if (!context) {
        log_error_simple("invalid validation context");
        return STATUS_INVALID_PARAMETER;
    }

    log_info_simple("starting validation process...");

    timeout.QuadPart = validation_timeout;

    status = event_manager_wait_for_validation(context, &timeout);

    if (status == status_key_validation_timeout) {
        log_error_simple("validation timed out - driver will unload");
        return status;
    }

    if (status == status_key_validation_failed) {
        log_error_simple("invalid key provided - driver will unload");
        return status;
    }

    if (!NT_SUCCESS(status)) {
        log_error("validation failed with status: 0x%X", status);
        return status;
    }

    log_success_simple("validation completed successfully - driver will continue");
    return STATUS_SUCCESS;
}

VOID validator_cleanup(p_validation_context context) {
    if (!context) {
        return;
    }

    log_info_simple("cleaning up key validator...");

    event_manager_cleanup(context);

    log_success_simple("key validator cleaned up");
}