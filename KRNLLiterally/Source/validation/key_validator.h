#pragma once

#ifndef KEY_VALIDATOR_H
#define KEY_VALIDATOR_H

#include <ntddk.h>
#include "../common/types.h"

NTSTATUS validator_initialize(p_validation_context context);
NTSTATUS validator_perform_validation(p_validation_context context);
VOID validator_cleanup(p_validation_context context);

#endif