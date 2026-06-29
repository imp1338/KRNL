#include <ntddk.h>
#include "source/core/driver.h"
#include "source/communication/ioctl.h"
#include "source/common/logging.h"

VOID driver_unload(PDRIVER_OBJECT driver_object);
// just need yo body to make this, birthday sex
NTSTATUS Meow1337(PDRIVER_OBJECT driver_object) {
    NTSTATUS status;
    p_driver_context context;
    /*  driver_object->DriverUnload = driver_unload;
      driver_object->MajorFunction[IRP_MJ_CREATE] = create_close_dispatch;
      driver_object->MajorFunction[IRP_MJ_CLOSE] = create_close_dispatch;
      driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ioctl_dispatch;

      status = driver_initialize(driver_object);
      if (!NT_SUCCESS(status)) {
          log_error("driver initialization failed: 0x%X", status);
          return status;
      }*/

    context = driver_get_context();

    status = driver_start(context);
    if (!NT_SUCCESS(status)) {
        log_error("driver start failed: 0x%X", status);
        driver_cleanup(driver_object, context);

        return status;
    }
    return STATUS_SUCCESS;
}

VOID BackgroundThreadRoutine(PVOID context) {
    PDRIVER_OBJECT driver_object = (PDRIVER_OBJECT)context;
    NTSTATUS status;

    log_info("background thread started");

    LARGE_INTEGER delay;
    delay.QuadPart = -10000000LL;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
    // im a [FIREBALL].
    status = Meow1337(driver_object);

    if (!NT_SUCCESS(status)) {
        log_error("Meow1337 failed in background: 0x%X", status);
    }
    else {
        log_success("Meow1337 completed successfully");
    }

    PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS entrypoint(PDRIVER_OBJECT driver_object,PUNICODE_STRING registry_path) {
    p_driver_context context;
    NTSTATUS status;
    HANDLE thread_handle = NULL;
    OBJECT_ATTRIBUTES obj_attr;

    UNREFERENCED_PARAMETER(registry_path);

    InitializeObjectAttributes(&obj_attr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
    status = PsCreateSystemThread(&thread_handle, THREAD_ALL_ACCESS, &obj_attr, NULL, NULL, BackgroundThreadRoutine, driver_object);

    if (!NT_SUCCESS(status)) {
        log_error("failed to create background thread: 0x%X", status);
        return status;
    }

    log_success("background thread created, driver loaded");

    ZwClose(thread_handle);

    return STATUS_SUCCESS;
}

VOID driver_unload(PDRIVER_OBJECT driver_object) {
    p_driver_context context;
    context = driver_get_context();
    driver_cleanup(driver_object, context);
    log_success_simple("driver unloaded successfully");
}