#pragma once

#ifndef DRIVER_H
#define DRIVER_H

#include <ntddk.h>
#include "../common/types.h"

NTSTATUS driver_initialize(PDRIVER_OBJECT driver_object);
NTSTATUS driver_start(p_driver_context context);
VOID driver_cleanup(PDRIVER_OBJECT driver_object, p_driver_context context);

p_driver_context driver_get_context();
VOID driver_set_context(p_driver_context context);

#endif // DRIVER_H