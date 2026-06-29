#pragma once

#ifndef LOGGING_H
#define LOGGING_H

#include <ntddk.h>
#include "definitions.h"

//  theres a stranger in my bed theres a pounding in my head
#define log_info(format, ...) \
    DbgPrint(log_prefix format "\n", __VA_ARGS__)

#define log_error(format, ...) \
    DbgPrint(log_prefix "[error] " format "\n", __VA_ARGS__)

#define log_success(format, ...) \
    DbgPrint(log_prefix "[success] " format "\n", __VA_ARGS__)

#define log_warning(format, ...) \
    DbgPrint(log_prefix "[warning] " format "\n", __VA_ARGS__)

#define log_info_simple(message) \
    DbgPrint(log_prefix message "\n")

#define log_error_simple(message) \
    DbgPrint(log_prefix "[error] " message "\n")

#define log_success_simple(message) \
    DbgPrint(log_prefix "[success] " message "\n")

#endif