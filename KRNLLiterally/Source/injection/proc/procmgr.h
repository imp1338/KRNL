#pragma once

#ifndef PROCMGR_H
#define PROCMGR_H

#include <ntifs.h>

PEPROCESS process_manager_find_by_name(const char* process_name);
NTSTATUS process_manager_get_process_id(PEPROCESS process, PHANDLE process_id);

// github.com/imp1338

#endif