#pragma once

#ifndef DEVICE_H
#define DEVICE_H

#include <ntddk.h>

NTSTATUS device_create(PDRIVER_OBJECT driver_object, PDEVICE_OBJECT* device_object);
NTSTATUS device_create_symlink();
VOID device_cleanup(PDEVICE_OBJECT device_object);

#endif