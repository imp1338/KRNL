#include <ntddk.h>
#include "ioctl.h"
#include "../common/definitions.h"
#include "../common/logging.h"
#include "../validation/event_mgr.h"

static p_validation_context g_validation_context = NULL;

VOID ioctl_set_validation_context(p_validation_context context) {
    g_validation_context = context;
}

NTSTATUS create_close_dispatch(PDEVICE_OBJECT device_object, PIRP irp) {
    UNREFERENCED_PARAMETER(device_object);

    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS ioctl_dispatch(PDEVICE_OBJECT device_object, PIRP irp) {
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irp_stack;
    ULONG io_control_code;
    PVOID buffer;
    ULONG input_length;

    UNREFERENCED_PARAMETER(device_object);

    irp_stack = IoGetCurrentIrpStackLocation(irp);
    io_control_code = irp_stack->Parameters.DeviceIoControl.IoControlCode;
    buffer = irp->AssociatedIrp.SystemBuffer;
    input_length = irp_stack->Parameters.DeviceIoControl.InputBufferLength;

    switch (io_control_code) {
    case ioctl_validate_key: {
        log_info_simple("received ioctl_validate_key");

        if (input_length < sizeof(BOOLEAN)) {
            log_error_simple("buffer too small for validation result");
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (!g_validation_context) {
            log_error_simple("validation context not initialized");
            status = STATUS_INVALID_DEVICE_STATE;
            break;
        }

        BOOLEAN* validation_result = (BOOLEAN*)buffer;

        log_info("validation result: %s", *validation_result ? "valid" : "invalid");

        event_manager_set_validation(g_validation_context, *validation_result);

        break;
    }

    default:
        log_warning("unknown ioctl code: 0x%X", io_control_code);
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    irp->IoStatus.Status = status;
    irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);

    return status;
}