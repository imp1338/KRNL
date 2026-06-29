#pragma once

#ifndef LOADER_STUB_H
#define LOADER_STUB_H

#include <ntddk.h>

struct LOADER_DATA {
    void* entry_point;
    void* image_base;
};

PVOID loader_stub_get_code();
SIZE_T loader_stub_get_size();

#endif