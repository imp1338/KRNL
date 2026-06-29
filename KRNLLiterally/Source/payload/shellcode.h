#pragma once

#ifndef SHELLCODE_H
#define SHELLCODE_H

#include <ntddk.h>

PVOID shellcode_get_buffer();
SIZE_T shellcode_get_size();

#endif