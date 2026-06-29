#pragma once

#ifndef EVENT_MGR_H
#define EVENT_MGR_H

#include <ntddk.h>
#include "../common/types.h"

VOID event_manager_initialize(p_validation_context context);
NTSTATUS event_manager_wait_for_validation(p_validation_context context, PLARGE_INTEGER timeout);
VOID event_manager_set_validation(p_validation_context context, BOOLEAN is_valid);
VOID event_manager_cleanup(p_validation_context context);

#endif