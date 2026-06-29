#pragma once

#ifndef IOCTL_H
#define IOCTL_H

#include <ntddk.h>
#include "../common/types.h"

NTSTATUS ioctl_dispatch(PDEVICE_OBJECT device_object, PIRP irp);
NTSTATUS create_close_dispatch(PDEVICE_OBJECT device_object, PIRP irp);

VOID ioctl_set_validation_context(p_validation_context context);

#endif