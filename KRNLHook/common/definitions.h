#pragma once

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <windows.h>

#define driver_symlink "\\\\.\\\\krnl"

#define ioctl_validate_key CTL_CODE( FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS )

#define correct_key "soarwazhere"

#endif