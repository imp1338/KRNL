#include <ntifs.h>
#include "procmgr.h"
#include "../../common/definitions.h"
#include "../../common/logging.h"

PEPROCESS process_manager_find_by_name(const char* process_name) {
    PEPROCESS current_process = NULL;
    PEPROCESS system_process = NULL;
    PLIST_ENTRY list_head = NULL;
    PLIST_ENTRY list_entry = NULL;

    if (!process_name) {
        log_error_simple("invalid process name");
        return NULL;
    }

    log_info("searching for process: %s", process_name);

    system_process = PsGetCurrentProcess();
    list_head = (PLIST_ENTRY)((PUCHAR)system_process + activeprocesslinks_offset);
    list_entry = list_head->Flink;

    while (list_entry != list_head) {
        current_process = (PEPROCESS)((PUCHAR)list_entry - activeprocesslinks_offset);
        char* image_name = (char*)((PUCHAR)current_process + imagefilename_offset);

        if (_stricmp(image_name, process_name) == 0) {
            ObReferenceObject(current_process);

            if (PsGetProcessExitStatus(current_process) != STATUS_PENDING) {
                ObDereferenceObject(current_process);
                break;
            }
            // i spend my time just thinking thinking thinking bout you
            log_success("found process: %s at 0x%p", process_name, current_process);
            return current_process;
        }

        list_entry = list_entry->Flink;
    }

    log_error("process not found or terminating: %s", process_name);
    return NULL;
}

NTSTATUS process_manager_get_process_id(PEPROCESS process, PHANDLE process_id) {
    if (!process || !process_id) {
        return STATUS_INVALID_PARAMETER;
    }

    *process_id = PsGetProcessId(process);
    return STATUS_SUCCESS;
}