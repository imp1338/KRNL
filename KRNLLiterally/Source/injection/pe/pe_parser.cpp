#include <ntifs.h>
#include "../mem/memalloc.h"
#include "../../common/logging.h"
#include "pe_parser.h"
#include <ntstrsafe.h>

PVOID get_module_base_in_target(PEPROCESS process, const char* module_name) {
    PPEB peb;
    KAPC_STATE apc;
    PVOID module_base = NULL;

    peb = PsGetProcessPeb(process);
    if (!peb) {
        return NULL;
    }

    ANSI_STRING ansi_search;
    UNICODE_STRING unicode_search;
    RtlInitAnsiString(&ansi_search, module_name);
    RtlAnsiStringToUnicodeString(&unicode_search, &ansi_search, TRUE);

    const char* apiset_mappings[][2] = {

        // if you aint got no money take yo broke ass home!
        {"api-ms-win-crt-stdio-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-runtime-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-string-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-heap-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-math-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-locale-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-time-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-convert-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-environment-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-filesystem-l1-1-0.dll", "ucrtbase.dll"},
        {"api-ms-win-crt-utility-l1-1-0.dll", "ucrtbase.dll"},
        {NULL, NULL}
    };

    for (int i = 0; apiset_mappings[i][0] != NULL; i++) {
        if (_stricmp(module_name, apiset_mappings[i][0]) == 0) {
            RtlFreeUnicodeString(&unicode_search);
            RtlInitAnsiString(&ansi_search, apiset_mappings[i][1]);
            RtlAnsiStringToUnicodeString(&unicode_search, &ansi_search, TRUE);
            break;
        }
    }

    KeStackAttachProcess(process, &apc);
    __try {
        PPEB_LDR_DATA ldr = peb->LoaderData;
        if (!ldr) {
            log_error_simple("PEB LoaderData is NULL");
            KeUnstackDetachProcess(&apc);
            RtlFreeUnicodeString(&unicode_search);
            return NULL;
        }

        ULONG module_count = 0;
        for (PLIST_ENTRY entry = ldr->InLoadOrderModuleList.Flink;
            entry != &ldr->InLoadOrderModuleList;
            entry = entry->Flink) {

            module_count++;

            PLDR_DATA_TABLE_ENTRY ldr_entry = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (ldr_entry->BaseDllName.Buffer && ldr_entry->BaseDllName.Length > 0) {
                ANSI_STRING ansi_name;
                if (NT_SUCCESS(RtlUnicodeStringToAnsiString(&ansi_name, &ldr_entry->BaseDllName, TRUE))) {
                    if (module_count <= 10) {
                       // log_info("module[%u]: %s (base: 0x%p)", module_count, ansi_name.Buffer, ldr_entry->DllBase);
                    }
                    RtlFreeAnsiString(&ansi_name);
                }

                if (RtlEqualUnicodeString(&ldr_entry->BaseDllName, &unicode_search, TRUE)) {
                   // log_success("found exact match for %s at 0x%p", module_name, ldr_entry->DllBase);
                    module_base = ldr_entry->DllBase;
                    break;
                }

                UNICODE_STRING name_without_ext = ldr_entry->BaseDllName;
                UNICODE_STRING search_without_ext = unicode_search;
                UNICODE_STRING dll_ext;
                RtlInitUnicodeString(&dll_ext, L".dll");


                if (unicode_search.Length >= dll_ext.Length) {
                    USHORT offset = unicode_search.Length - dll_ext.Length;
                    UNICODE_STRING search_end;
                    search_end.Buffer = (PWCH)((PUCHAR)unicode_search.Buffer + offset);
                    search_end.Length = dll_ext.Length;
                    search_end.MaximumLength = dll_ext.Length;

                    if (RtlEqualUnicodeString(&search_end, &dll_ext, TRUE)) {
                        search_without_ext.Length = offset;
                    }
                }

                // double sanity check kekw

                if (name_without_ext.Length >= dll_ext.Length) {
                    USHORT offset = name_without_ext.Length - dll_ext.Length;
                    UNICODE_STRING name_end;
                    name_end.Buffer = (PWCH)((PUCHAR)name_without_ext.Buffer + offset);
                    name_end.Length = dll_ext.Length;
                    name_end.MaximumLength = dll_ext.Length;

                    if (RtlEqualUnicodeString(&name_end, &dll_ext, TRUE)) {
                        name_without_ext.Length = offset;
                    }
                }

                if (RtlEqualUnicodeString(&name_without_ext, &search_without_ext, TRUE)) {
                    log_success("found match (without ext) for %s at 0x%p", module_name, ldr_entry->DllBase);
                    module_base = ldr_entry->DllBase;
                    break;
                }
            }

            if (module_count > 1000) {
                log_error_simple("too many modules, breaking loop");
                break;
            }
        }

        if (!module_base) {
            log_error("module %s not found after checking %u modules", module_name, module_count);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        log_error("exception in get_module_base_in_target: 0x%X", GetExceptionCode());
        module_base = NULL;
    }

    KeUnstackDetachProcess(&apc);
    RtlFreeUnicodeString(&unicode_search);

    return module_base;
}

PVOID get_export_address_by_ordinal(
    PEPROCESS target_process,
    PVOID module_base,
    USHORT ordinal
)
{
    if (!target_process || !module_base) {
        return NULL;
    }

    KAPC_STATE apc_state = { 0 };
    PVOID result = NULL;
    SIZE_T bytes_copied = 0;
    NTSTATUS status;

    __try {
        KeStackAttachProcess(target_process, &apc_state);


        IMAGE_DOS_HEADER dos_header = { 0 };
        status = MmCopyVirtualMemory(target_process,module_base,PsGetCurrentProcess(),&dos_header,sizeof(IMAGE_DOS_HEADER),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_DOS_HEADER)) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        IMAGE_NT_HEADERS64 nt_headers = { 0 };
        PVOID nt_headers_addr = (PUCHAR)module_base + dos_header.e_lfanew;

        status = MmCopyVirtualMemory(target_process,nt_headers_addr,PsGetCurrentProcess(),&nt_headers,sizeof(IMAGE_NT_HEADERS64),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_NT_HEADERS64)) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        if (nt_headers.Signature != IMAGE_NT_SIGNATURE) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        ULONG export_dir_rva = nt_headers.OptionalHeader.DataDirectory[0].VirtualAddress;
        ULONG export_dir_size = nt_headers.OptionalHeader.DataDirectory[0].Size;

        if (!export_dir_rva || !export_dir_size) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        IMAGE_EXPORT_DIRECTORY export_dir = { 0 };
        PVOID export_dir_addr = (PUCHAR)module_base + export_dir_rva;

        status = MmCopyVirtualMemory(target_process,export_dir_addr,PsGetCurrentProcess(),&export_dir,sizeof(IMAGE_EXPORT_DIRECTORY),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_EXPORT_DIRECTORY)) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        if (ordinal < export_dir.Base || ordinal >= export_dir.Base + export_dir.NumberOfFunctions) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }
        // i want you so bad its my only wish

        ULONG function_index = ordinal - export_dir.Base;

        if (function_index >= export_dir.NumberOfFunctions) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        ULONG function_rva = 0;
        PVOID address_table_entry = (PUCHAR)module_base + export_dir.AddressOfFunctions + (function_index * sizeof(ULONG));

        status = MmCopyVirtualMemory(target_process,address_table_entry,PsGetCurrentProcess(),&function_rva,sizeof(ULONG),KernelMode,&bytes_copied);
        if (!NT_SUCCESS(status) || bytes_copied != sizeof(ULONG)) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        if (!function_rva) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        if (function_rva >= export_dir_rva && function_rva < export_dir_rva + export_dir_size) {
            KeUnstackDetachProcess(&apc_state);
            return NULL;
        }

        result = (PUCHAR)module_base + function_rva;
        KeUnstackDetachProcess(&apc_state);

        return result;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (apc_state.Process != NULL) {
            KeUnstackDetachProcess(&apc_state);
        }
        return NULL;
    }
}

PVOID get_export_address(PEPROCESS target_process, PVOID module_base, const char* export_name) {
    if (!target_process || !module_base || !export_name) {
        log_error_simple("get_export_address: invalid parameters");
        return NULL;
    }

    log_info("searching for export: %s in module: 0x%p", export_name, module_base);

    KAPC_STATE apc_state = { 0 };
    PVOID result = NULL;
    NTSTATUS status;
    SIZE_T bytes_copied = 0;

    PUCHAR headers = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'epmH');
    if (!headers) {
        log_error_simple("failed to allocate headers buffer");
        return NULL;
    }


    // I CANT GO ANY FURTHER THAN THIS.
    __try {
        KeStackAttachProcess(target_process, &apc_state);
        log_success_simple("attached to target process");

        IMAGE_DOS_HEADER dos_header = { 0 };
        status = MmCopyVirtualMemory(target_process,module_base,PsGetCurrentProcess(),&dos_header,sizeof(IMAGE_DOS_HEADER),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_DOS_HEADER)) {
            log_error("failed to read DOS header: 0x%X", status);
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        if (dos_header.e_magic != IMAGE_DOS_SIGNATURE) {
            log_error("invalid DOS signature: 0x%X", dos_header.e_magic);
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success_simple("DOS header valid");

        IMAGE_NT_HEADERS64 nt_headers = { 0 };
        PVOID nt_headers_addr = (PUCHAR)module_base + dos_header.e_lfanew;

        status = MmCopyVirtualMemory(target_process,nt_headers_addr,PsGetCurrentProcess(),&nt_headers,sizeof(IMAGE_NT_HEADERS64),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_NT_HEADERS64)) {
            log_error("failed to read NT headers: 0x%X", status);
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        if (nt_headers.Signature != IMAGE_NT_SIGNATURE) {
            log_error("invalid NT signature: 0x%X", nt_headers.Signature);
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success_simple("NT headers valid");
        log_info("OptionalHeader.Magic: 0x%X", nt_headers.OptionalHeader.Magic);
        log_info("OptionalHeader.SizeOfImage: 0x%X", nt_headers.OptionalHeader.SizeOfImage);

        if (nt_headers.OptionalHeader.Magic != 0x20b) {
            log_error("not a 64-bit image (Magic: 0x%X, expected 0x%X)",nt_headers.OptionalHeader.Magic, 0x20b);
            return NULL;
        }

        ULONG export_dir_rva = nt_headers.OptionalHeader.DataDirectory[0].VirtualAddress;
        ULONG export_dir_size = nt_headers.OptionalHeader.DataDirectory[0].Size;

        log_info("export directory: RVA=0x%X Size=0x%X", export_dir_rva, export_dir_size);

        if (!export_dir_rva || !export_dir_size) {
            log_error_simple("no export directory found");
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        ULONG name_rva = 0;
        CHAR module_name[256] = { 0 };

        status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + export_dir_rva + 0x0C,PsGetCurrentProcess(),&name_rva,sizeof(ULONG),KernelMode,&bytes_copied);

        if (NT_SUCCESS(status) && name_rva > 0 && name_rva < nt_headers.OptionalHeader.SizeOfImage) {
            status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + name_rva,PsGetCurrentProcess(),module_name,sizeof(module_name) - 1,KernelMode,&bytes_copied);

            if (NT_SUCCESS(status)) {
                module_name[sizeof(module_name) - 1] = '\0';
                log_info("module name from export dir: %s", module_name);
            }
        }

        // rva is relative to imb
        // ^- nerd
        IMAGE_EXPORT_DIRECTORY export_dir = { 0 };
        PVOID export_dir_addr = (PUCHAR)module_base + export_dir_rva;

        log_info("reading export directory from address: 0x%p (module_base 0x%p + RVA 0x%X)",export_dir_addr, module_base, export_dir_rva);

        status = MmCopyVirtualMemory(target_process,export_dir_addr,PsGetCurrentProcess(),&export_dir,sizeof(IMAGE_EXPORT_DIRECTORY),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status) || bytes_copied != sizeof(IMAGE_EXPORT_DIRECTORY)) {
            log_error("failed to read export directory: 0x%X (bytes copied: %zu)", status, bytes_copied);
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        if (export_dir.NumberOfFunctions > 100000 || export_dir.NumberOfNames > 100000) {
            log_error("export directory appears corrupted (too many functions/names)");
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        ULONG max_functions = export_dir.NumberOfFunctions;
        ULONG max_names = export_dir.NumberOfNames;

        if (max_functions == 0 || max_names == 0) {
            log_error_simple("export directory has no functions or names");
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        PULONG function_addresses = (PULONG)ExAllocatePoolWithTag(NonPagedPool, max_functions * sizeof(ULONG), 'afeX');
        PULONG name_addresses = (PULONG)ExAllocatePoolWithTag(NonPagedPool, max_names * sizeof(ULONG), 'aneX');
        PUSHORT name_ordinals = (PUSHORT)ExAllocatePoolWithTag(NonPagedPool, max_names * sizeof(USHORT), 'oneX');

        if (!function_addresses || !name_addresses || !name_ordinals) {
            log_error_simple("failed to allocate export table buffers");
            if (function_addresses) ExFreePoolWithTag(function_addresses, 'afeX');
            if (name_addresses) ExFreePoolWithTag(name_addresses, 'aneX');
            if (name_ordinals) ExFreePoolWithTag(name_ordinals, 'oneX');
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success_simple("allocated export table buffers");
        log_info("reading function address table from RVA 0x%X", export_dir.AddressOfFunctions);
        status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + export_dir.AddressOfFunctions,PsGetCurrentProcess(),function_addresses,max_functions * sizeof(ULONG),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status)) {
            log_error("failed to read function address table: 0x%X", status);
            ExFreePoolWithTag(function_addresses, 'afeX');
            ExFreePoolWithTag(name_addresses, 'aneX');
            ExFreePoolWithTag(name_ordinals, 'oneX');
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success("read %zu bytes from function address table", bytes_copied);

        log_info("reading name address table from RVA 0x%X", export_dir.AddressOfNames);
        status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + export_dir.AddressOfNames,PsGetCurrentProcess(),name_addresses,max_names * sizeof(ULONG),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status)) {
            log_error("failed to read name address table: 0x%X", status);
            ExFreePoolWithTag(function_addresses, 'afeX');
            ExFreePoolWithTag(name_addresses, 'aneX');
            ExFreePoolWithTag(name_ordinals, 'oneX');
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success("read %zu bytes from name address table", bytes_copied);
        log_info("reading name ordinal table from RVA 0x%X", export_dir.AddressOfNameOrdinals);

        status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + export_dir.AddressOfNameOrdinals,PsGetCurrentProcess(),name_ordinals,max_names * sizeof(USHORT),KernelMode,&bytes_copied);

        if (!NT_SUCCESS(status)) {
            log_error("failed to read name ordinal table: 0x%X", status);
            ExFreePoolWithTag(function_addresses, 'afeX');
            ExFreePoolWithTag(name_addresses, 'aneX');
            ExFreePoolWithTag(name_ordinals, 'oneX');
            // CAN YOU MEET ME HALFWAY RIGHT AT THE BORDERLINE IS WHERE IM GONNA WAIT. FOR YOU!
            KeUnstackDetachProcess(&apc_state);
            ExFreePoolWithTag(headers, 'epmH');
            return NULL;
        }

        log_success("read %zu bytes from name ordinal table", bytes_copied);
        log_info("starting export name search for: %s", export_name);

        for (ULONG i = 0; i < max_names; i++) {
            CHAR name_buffer[256] = { 0 };

            status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + name_addresses[i],PsGetCurrentProcess(),name_buffer,sizeof(name_buffer) - 1,KernelMode,&bytes_copied);

            if (!NT_SUCCESS(status)) {
                log_error("failed to read export name at index %u: 0x%X", i, status);
                continue;
            }
            name_buffer[sizeof(name_buffer) - 1] = '\0';
            if (i < 5) {
                //log_info("export[%u]: %s", i, name_buffer);
            }

            if (strcmp(name_buffer, export_name) == 0) {
                //log_success("found matching export at index %u: %s", i, export_name);

                USHORT ordinal_index = name_ordinals[i];
                //log_info("ordinal index: %u", ordinal_index);

                if (ordinal_index >= max_functions) {
                    //log_error("ordinal index %u exceeds max functions %u", ordinal_index, max_functions);
                    break;
                }

                ULONG function_rva = function_addresses[ordinal_index];
                //log_info("function RVA: 0x%X", function_rva);

                if (function_rva == 0) {
                    log_error_simple("function RVA is zero");
                    break;
                }

                if (function_rva >= export_dir_rva && function_rva < export_dir_rva + export_dir_size) {
                   // log_info("export is forwarded (RVA 0x%X in range 0x%X-0x%X)",function_rva, export_dir_rva, export_dir_rva + export_dir_size);
                    CHAR forward_buffer[256] = { 0 };

                    status = MmCopyVirtualMemory(target_process,(PUCHAR)module_base + function_rva,PsGetCurrentProcess(),forward_buffer,sizeof(forward_buffer) - 1,KernelMode,&bytes_copied);

                    if (NT_SUCCESS(status)) {
                        forward_buffer[sizeof(forward_buffer) - 1] = '\0';
                     //   log_info("export %s is forwarded to: %s", export_name, forward_buffer);

                        char* dot = strchr(forward_buffer, '.');
                        if (dot) {
                            *dot = '\0';
                            char* forward_dll = forward_buffer;
                            char* forward_func = dot + 1;

                           // log_info("forward target: DLL=%s Function=%s", forward_dll, forward_func);

                            char dll_name_buf[256];
                            if (strstr(forward_dll, ".dll") == NULL && strstr(forward_dll, ".DLL") == NULL) {
                                sprintf(dll_name_buf, "%s.dll", forward_dll);
                            }
                            else {
                                strcpy(dll_name_buf, forward_dll);
                            }

                           // log_info("looking for forwarded module: %s", dll_name_buf);

                            PVOID forward_module = get_module_base_in_target(target_process, dll_name_buf);
                            if (forward_module) {
                                log_info("found forwarded module at 0x%p, resolving %s", forward_module, forward_func);
                                result = get_export_address(target_process, forward_module, forward_func);
                                if (result) {
                                   // log_success("resolved forwarded %s -> 0x%p", export_name, result);
                                }
                                else {
                                    log_error("failed to resolve forwarded function: %s", forward_func);
                                }
                            }
                            else {
                                log_error("failed to find forwarded module: %s", dll_name_buf);
                            }
                        }
                        else {
                            log_error("invalid forward string format: %s", forward_buffer);
                        }
                    }
                    else {
                        log_error("failed to read forward string: 0x%X", status);
                    }
                    break;
                }

                result = (PUCHAR)module_base + function_rva;
                log_success("resolved %s -> 0x%p (RVA: 0x%X)", export_name, result, function_rva);
                break;
            }
        }

        if (!result) {
            log_error("export %s not found after searching %u names", export_name, max_names);
        }

        ExFreePoolWithTag(function_addresses, 'afeX');
        ExFreePoolWithTag(name_addresses, 'aneX');
        ExFreePoolWithTag(name_ordinals, 'oneX');

        //i gotta feeling, that tonights gonna be a good night !
        KeUnstackDetachProcess(&apc_state);
        log_success_simple("detached from target process");
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        log_error("exception in get_export_address: 0x%X", GetExceptionCode());
        if (apc_state.Process != NULL) {
            KeUnstackDetachProcess(&apc_state);
        }
        result = NULL;
    }

    ExFreePoolWithTag(headers, 'epmH');
    return result;
}

uintptr_t rva_to_file_offset(PIMAGE_NT_HEADERS64 nt_headers,uintptr_t rva)
{
    PIMAGE_SECTION_HEADER section_header = (PIMAGE_SECTION_HEADER)((PUCHAR)nt_headers + sizeof(IMAGE_NT_HEADERS64));
    if (rva < nt_headers->OptionalHeader.SizeOfHeaders) {
        return rva;
    }
    for (USHORT i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
        uintptr_t section_start = section_header[i].VirtualAddress;
        uintptr_t section_end = section_start + section_header[i].Misc.VirtualSize;

        if (rva >= section_start && rva < section_end) {
            return rva - section_start + section_header[i].PointerToRawData;
        }
    }
    return 0xFFFFFFFF;
}

NTSTATUS pe_loader_map_dll(PEPROCESS target_process,PVOID dll_buffer,size_t dll_size,p_manual_map_context map_context)
{
    NTSTATUS status;
    PIMAGE_DOS_HEADER dos_header;
    PIMAGE_NT_HEADERS64 nt_headers;
    PIMAGE_SECTION_HEADER section_header;
    PVOID image_base = NULL;
    size_t image_size;

    if (!target_process || !dll_buffer || !map_context) {
        log_error_simple("invalid parameters for pe loader");
        return STATUS_INVALID_PARAMETER;
    }

    dos_header = (PIMAGE_DOS_HEADER)dll_buffer;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        log_error_simple("invalid dos signature");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    nt_headers = (PIMAGE_NT_HEADERS64)((PUCHAR)dll_buffer + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        log_error_simple("invalid nt signature");
        return STATUS_INVALID_IMAGE_FORMAT;
    }

    image_size = nt_headers->OptionalHeader.SizeOfImage;

    //log_info("mapping dll: size = 0x%zx", image_size);

    status = memory_allocator_allocate(target_process, image_size, &image_base);
    if (!NT_SUCCESS(status)) {
        log_error("failed to allocate image memory: 0x%X", status);
        return status;
    }

    log_info("allocated image base at: 0x%p", image_base);

    status = memory_allocator_write(target_process,image_base,dll_buffer,nt_headers->OptionalHeader.SizeOfHeaders);

    if (!NT_SUCCESS(status)) {
        log_error("failed to write headers: 0x%X", status);
        memory_allocator_free(target_process, image_base);
        return status;
    }

    log_info_simple("wrote pe headers");

    // all i can do is try, give me one chance, whats the problem ? i dont see a ring on ya hand

    section_header = (PIMAGE_SECTION_HEADER)((PUCHAR)nt_headers + sizeof(IMAGE_NT_HEADERS64));

    for (USHORT i = 0; i < nt_headers->FileHeader.NumberOfSections; i++) {
        if (section_header[i].SizeOfRawData == 0) {
            continue;
        }

        PVOID section_dest = (PUCHAR)image_base + section_header[i].VirtualAddress;
        PVOID section_src = (PUCHAR)dll_buffer + section_header[i].PointerToRawData;

        log_info("mapping section %d: VA=0x%X Size=0x%X",i,section_header[i].VirtualAddress,section_header[i].SizeOfRawData);

        status = memory_allocator_write(target_process,section_dest,section_src,section_header[i].SizeOfRawData);

        if (!NT_SUCCESS(status)) {
            log_error("failed to write section %d: 0x%X", i, status);
            memory_allocator_free(target_process, image_base);
            return status;
        }
    }

    log_success_simple("all sections mapped successfully");

    if (nt_headers->OptionalHeader.DataDirectory[1].Size) {
        DWORD import_rva = nt_headers->OptionalHeader.DataDirectory[1].VirtualAddress;
        DWORD import_file_offset = rva_to_file_offset(nt_headers, import_rva);

        if (import_file_offset == 0xFFFFFFFF || import_file_offset >= dll_size) {
            log_error("import directory RVA 0x%X -> file offset 0x%X out of bounds (dll_size: 0x%zx)",import_rva, import_file_offset, dll_size);
        }
        else {
            //log_info("import directory: RVA=0x%X FileOffset=0x%X", import_rva, import_file_offset);

            PIMAGE_IMPORT_DESCRIPTOR import_desc = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)dll_buffer + import_file_offset);

            __try {
                for (DWORD i = 0; ; i++) {
                    PIMAGE_IMPORT_DESCRIPTOR current = &import_desc[i];

                    if ((PUCHAR)current + sizeof(IMAGE_IMPORT_DESCRIPTOR) > (PUCHAR)dll_buffer + dll_size) {
                        log_success_simple("reached end of import table (bounds)");
                        break;
                    }

                    if (current->Name == 0 && current->FirstThunk == 0) {
                        log_success_simple("import resolution complete");
                        break;
                    }

                    DWORD name_file_offset = rva_to_file_offset(nt_headers, current->Name);
                    if (name_file_offset == 0xFFFFFFFF || name_file_offset >= dll_size) {
                        log_error("import name RVA 0x%X out of bounds", current->Name);
                        continue;
                    }

                    char* dll_name = (char*)((PUCHAR)dll_buffer + name_file_offset);

                    SIZE_T max_len = dll_size - name_file_offset;
                    SIZE_T name_len = strnlen(dll_name, max_len);
                    if (name_len == 0 || name_len == max_len) {
                        log_error_simple("invalid dll name");
                        continue;
                    }

                    // promiscous girl wherever you are, im all alone, and its you that i want ;)

                    log_info("resolving imports for: %s", dll_name);

                    PVOID module_base = get_module_base_in_target(target_process, dll_name);
                    if (!module_base) {
                        log_error("module not found in target: %s", dll_name);
                        continue;
                    }

                    //log_info("got module_base 0x%p for %s", module_base, dll_name);

					// kernelbase check for kernel32 imports
                    PVOID kernelbase_module = NULL;
                    if (_stricmp(dll_name, "KERNEL32.dll") == 0 || _stricmp(dll_name, "KERNEL32.DLL") == 0) {
                        kernelbase_module = get_module_base_in_target(target_process, "KERNELBASE.dll");
                        if (kernelbase_module) {
                            log_info("found KERNELBASE.dll at 0x%p, will try it first for KERNEL32 imports", kernelbase_module);
                        }
                    }

                    DWORD thunk_rva = current->OriginalFirstThunk ? current->OriginalFirstThunk : current->FirstThunk;
                    DWORD iat_rva = current->FirstThunk;
                    DWORD thunk_file_offset = rva_to_file_offset(nt_headers, thunk_rva);

                    if (thunk_file_offset == 0xFFFFFFFF || thunk_file_offset >= dll_size) {
                        log_error("thunk RVA 0x%X out of bounds", thunk_rva);
                        // PROMISCOUS BOY YOU ALREADY KNOW IM ALL YOURS, WHAT YOU WAITING FOR
                        continue;
                    }

                    PIMAGE_THUNK_DATA64 thunk = (PIMAGE_THUNK_DATA64)((PUCHAR)dll_buffer + thunk_file_offset);
                    PIMAGE_THUNK_DATA64 iat = (PIMAGE_THUNK_DATA64)((PUCHAR)image_base + iat_rva);

                    for (DWORD j = 0; ; j++) {
                        PIMAGE_THUNK_DATA64 current_thunk = &thunk[j];
                        PIMAGE_THUNK_DATA64 current_iat = &iat[j];

                        if ((PUCHAR)current_thunk + sizeof(IMAGE_THUNK_DATA64) > (PUCHAR)dll_buffer + dll_size) {
                            break;
                        }

                        if (current_thunk->u1.AddressOfData == 0) {
                            break;
                        }

                        PVOID func_addr = NULL;
                        if (IMAGE_SNAP_BY_ORDINAL64(current_thunk->u1.Ordinal)) {
                            USHORT ordinal = (USHORT)IMAGE_ORDINAL64(current_thunk->u1.Ordinal);
                            func_addr = get_export_address_by_ordinal(target_process, module_base, ordinal);
                            if (func_addr) {
                                log_info("resolved ordinal %d -> 0x%p", ordinal, func_addr);
                            }
                            else {
                                log_error("failed to resolve ordinal %d", ordinal);
                            }
                        }
                        else {
                            DWORD import_name_rva = (DWORD)current_thunk->u1.AddressOfData;
                            DWORD import_name_file_offset = rva_to_file_offset(nt_headers, import_name_rva);

                            if (import_name_file_offset != 0xFFFFFFFF &&
                                import_name_file_offset + sizeof(IMAGE_IMPORT_BY_NAME) <= dll_size) {
                                PIMAGE_IMPORT_BY_NAME import_by_name = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)dll_buffer + import_name_file_offset);

                                SIZE_T func_max_len = dll_size - (import_name_file_offset + FIELD_OFFSET(IMAGE_IMPORT_BY_NAME, Name));
                                if (strnlen(import_by_name->Name, func_max_len) < func_max_len) {
                                    if (kernelbase_module) {
                                        func_addr = get_export_address(target_process, kernelbase_module, import_by_name->Name);
                                        if (func_addr) {
                                           // log_info("resolved %s from KERNELBASE -> 0x%p", import_by_name->Name, func_addr);
                                        }
                                    }
                                    if (!func_addr) {
                                        func_addr = get_export_address(target_process, module_base, import_by_name->Name);
                                        if (func_addr) {
                                            //log_info("resolved function: %s -> 0x%p", import_by_name->Name, func_addr);
                                        }
                                        else {
                                            log_error("failed to resolve: %s", import_by_name->Name);
                                        }
                                    }
                                }
                            }
                        }
                        if (func_addr) {
                            status = memory_allocator_write(target_process, current_iat, &func_addr, sizeof(PVOID));
                            if (!NT_SUCCESS(status)) {
                                log_error("failed to write IAT entry: 0x%X", status);
                            }
                        }
                        else {
                            log_error_simple("failed to resolve import");
                        }
                    }
                }
                // YOU SAY YOU WANT PASSION, I THINK YOU FOUDN IT, GET READY FOR ACTIOn, DONT BE ASTOUNDED, WE SWITCHING POSITIONS YOU FEEL SORROUNDED
                // TELL ME WHERE YOU WANT YOU GIIIFT GIRL
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                log_error_simple("exception during import resolution");
            }
        }
    }

    map_context->image_base = image_base;
    map_context->image_size = image_size;
    map_context->entry_point = (PUCHAR)image_base + nt_headers->OptionalHeader.AddressOfEntryPoint;

    log_info("entry point at: 0x%p", map_context->entry_point);

    return STATUS_SUCCESS;
}

VOID pe_loader_cleanup(PEPROCESS target_process,p_manual_map_context map_context) 
{
    if (!map_context || !target_process) {
        return;
    }

    if (map_context->image_base) {
        memory_allocator_free(target_process, map_context->image_base);
        map_context->image_base = NULL;
        // girl you know ahahahaha
    }
}