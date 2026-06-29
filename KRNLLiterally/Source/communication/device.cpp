#include <ntddk.h>
#include "device.h"
#include "../common/definitions.h"
#include "../common/logging.h"


// G-L-A-M-O-R-O-U-S

NTSTATUS device_create(PDRIVER_OBJECT driver_object, PDEVICE_OBJECT* device_object) {
    NTSTATUS status;
    UNICODE_STRING device_name_unicode;

    RtlInitUnicodeString(&device_name_unicode, device_name);

    log_info("device name: %wZ", &device_name_unicode);
    log_info_simple("creating device object...");

    status = IoCreateDevice(driver_object,0,&device_name_unicode,FILE_DEVICE_UNKNOWN,FILE_DEVICE_SECURE_OPEN,FALSE,device_object);
    log_info("IoCreateDevice returned status: 0x%X", status);

    if (!NT_SUCCESS(status)) {
        log_error("failed to create device object: 0x%X", status);
        return status;
    }

    log_success_simple("device object created successfully");
    return STATUS_SUCCESS;
}

NTSTATUS device_create_symlink() {
    NTSTATUS status;
    UNICODE_STRING device_name_unicode;
    UNICODE_STRING symlink_name_unicode;

    RtlInitUnicodeString(&device_name_unicode, device_name);
    RtlInitUnicodeString(&symlink_name_unicode, symlink_name);

    log_info_simple("creating symbolic link...");

    status = IoCreateSymbolicLink(&symlink_name_unicode, &device_name_unicode);

    if (!NT_SUCCESS(status)) {
        log_error("failed to create symbolic link: 0x%X", status);
        return status;
    }

    log_success_simple("symbolic link created successfully");
    return STATUS_SUCCESS;
}

VOID device_cleanup(PDEVICE_OBJECT device_object) {
    UNICODE_STRING symlink_name_unicode;

    log_info_simple("cleaning up device...");

    RtlInitUnicodeString(&symlink_name_unicode, symlink_name);
    IoDeleteSymbolicLink(&symlink_name_unicode);

    if (device_object) {
        IoDeleteDevice(device_object);
    }

    log_success_simple("device cleanup completed");
}