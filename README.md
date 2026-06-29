# KRNL

**KRNL** is a kernel-mode proof-of-concept that demonstrates complete DLL injection entirely from ring-0, with a built-in key validation system. All injection (memory allocation, PE parsing, import resolution, writing, and entry point execution) happens entirely in kernel mode — no user-mode component required.

---

## Overview

KRNL performs full DLL injection **entirely in kernel mode** with a key validation system:

- Allocates memory in the target process from kernel
- Writes the DLL to the remote process
- Parses PE headers and resolves imports
- Executes the DLL entry point via remote thread
- Implements a key validation system via IOCTL
- No user-mode module required

The driver injects a payload DLL into a target process, which then waits for a key validation. The driver communicates with the payload through a shared event, and only proceeds with privileged operations after successful validation.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         KERNEL MODE (Ring-0)                        │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                    KRNLLiterally.sys                            ││
│  │  ┌───────────────────────────────────────────────────────────┐  ││
│  │  │                   Injection Engine                        │  ││
│  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐  │  ││
│  │  │  │  Memalloc   │ │   PE Parser │ │  Shell Injection    │  │  ││
│  │  │  │  - Allocate │ │  - Parse PE │ │  - Create remote    │  │  ││
│  │  │  │  - Write    │ │  - Resolve  │ │    thread           │  │  ││
│  │  │  │  - Free     │ │    imports  │ │  - Execute entry    │  │  ││
│  │  │  └─────────────┘ └─────────────┘ └─────────────────────┘  │  ││
│  │  └───────────────────────────────────────────────────────────┘  ││
│  │  ┌───────────────────────────────────────────────────────────┐  ││
│  │  │                   Key Validation System                   │  ││
│  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐  │  ││
│  │  │  │ Event Mgr   │ │ Key Validate│ │     IOCTL Handler   │  │  ││
│  │  │  │ - Initialize│ │ - Validate  │ │  - Receive key      │  │  ││
│  │  │  │ - Wait      │ │ - Timeout   │ │  - Set validation   │  │  ││
│  │  │  └─────────────┘ └─────────────┘ └─────────────────────┘  │  ││
│  │  └───────────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────┘│
│                              │                                      │
│                              ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                   Target Process (Ring-3)                       ││
│  │  ┌───────────────────────────────────────────────────────────┐  ││
│  │  │              Injected DLL (Payload)                       │  ││
│  │  │  - Creates validation window                              │  ││
│  │  │  - Waits for key input                                    │  ││
│  │  │  - Returns key result via IOCTL                           │  ││
│  │  │  - Validates key                                          │  ││
│  │  └───────────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────┘
```

---

## Components

### Core (`Source/core/`)

| Component | Description |
|-----------|-------------|
| **driver.cpp** | Main driver logic — initialization, process waiting, injection trigger |
| **driver.h** | Driver context management and function prototypes |
| **entrypoint.cpp** | Driver entry point, background thread creation, unload handling |

**Key Functions:**
- `entrypoint()` — Driver entry point, creates background thread
- `driver_initialize()` — Sets up device, symbolic link, and validator
- `driver_start()` — Waits for target process and triggers injection
- `driver_unload()` — Cleans up resources on unload
- `AwaitProcess()` — Polls for target process (mspaint.exe) with beep feedback

### Communication (`Source/communication/`)

| Component | Description |
|-----------|-------------|
| **device.cpp** | Creates device object and symbolic link (`\\.\krnl`) |
| **ioctl.cpp** | Handles IOCTL dispatch for user-mode communication |

**IOCTL Codes:**
| Code | Description |
|------|-------------|
| `ioctl_validate_key` | Receives validation result from user-mode (BOOLEAN) |

### Injection Engine (`Source/injection/`)

#### Memory Manager (`mem/`)
- `memory_allocator_allocate()` — Allocates memory in target process via `ZwAllocateVirtualMemory`
- `memory_allocator_write()` — Writes data to target process memory
- `memory_allocator_read()` — Reads data from target process memory
- `memory_allocator_free()` — Frees allocated memory

#### PE Parser (`pe/`)
- `pe_loader_map_dll()` — Full manual mapping of DLL:
  - Parses DOS and NT headers
  - Maps sections to allocated memory
  - Resolves imports (with API set remapping)
  - Handles forwarded exports
- `get_module_base_in_target()` — Finds module base in target process
- `get_export_address()` — Resolves export by name
- `get_export_address_by_ordinal()` — Resolves export by ordinal

**API Set Remapping:**
```cpp
{"api-ms-win-crt-stdio-l1-1-0.dll", "ucrtbase.dll"},
{"api-ms-win-crt-runtime-l1-1-0.dll", "ucrtbase.dll"},
// ... 10+ remappings
```

#### Process Manager (`proc/`)
- `process_manager_find_by_name()` — Finds process by name in EPROCESS list
- Uses offsets for:
  - `activeprocesslinks_offset` (0x448 on x64)
  - `imagefilename_offset` (0x5A8 on x64)

#### Shell Injection (`shell/`)
- `perform_injection()` — Main injection routine:
  1. Finds target process
  2. Opens process handle
  3. Maps DLL via PE loader
  4. Allocates loader stub and data
  5. Creates remote thread to execute stub
- `injector_cleanup()` — Cleans up injection resources

#### Stub (`stub/`)
- **Loader stub** — Small assembly stub that calls DLL entry point:
```asm
sub rsp, 0x28
mov rax, [rcx]      ; Load entry_point from LOADER_DATA
mov rcx, [rcx+8]    ; Load image_base as parameter
mov edx, 1          ; DLL_PROCESS_ATTACH
xor r8, r8
call rax
add rsp, 0x28
ret
```

### Validation System (`Source/validation/`)

| Component | Description |
|-----------|-------------|
| **event_mgr.cpp** | Manages validation event (KEVENT) for synchronization |
| **key_validator.cpp** | Validates key and handles timeout |

**Key Functions:**
- `event_manager_initialize()` — Initializes KEVENT
- `event_manager_wait_for_validation()` — Waits for validation with timeout (180 seconds)
- `event_manager_set_validation()` — Sets validation result and signals event
- `validator_perform_validation()` — Performs full validation flow

### Payload (`Source/payload/`)

- `shellcode.cpp` — Contains the DLL payload (18432 bytes)
- The payload is the DLL that gets injected into the target process

### Common (`Source/common/`)

| File | Description |
|------|-------------|
| **definitions.h** | Constants, device names, offsets, IOCTL codes, NTSTATUS codes |
| **types.h** | Core type definitions (validation_context, injection_context, driver_context) |
| **logging.h** | Kernel logging macros (log_info, log_error, log_success, etc.) |

---

## Project Structure

```
KRNL/
├── KRNLLiterally/
│   ├── Includes/
│   │   └── KRNLLiterally.inf         # Driver INF file
│   ├── Source/
│   │   ├── common/
│   │   │   ├── definitions.h         # Constants & offsets
│   │   │   ├── logging.h             # Logging macros
│   │   │   └── types.h               # Core type definitions
│   │   ├── communication/
│   │   │   ├── device.cpp            # Device creation
│   │   │   ├── device.h
│   │   │   ├── ioctl.cpp             # IOCTL dispatch
│   │   │   └── ioctl.h
│   │   ├── core/
│   │   │   ├── driver.cpp            # Main driver logic
│   │   │   └── driver.h
│   │   ├── injection/
│   │   │   ├── mem/
│   │   │   │   ├── memalloc.cpp      # Memory operations
│   │   │   │   └── memalloc.h
│   │   │   ├── pe/
│   │   │   │   ├── pe_parser.cpp     # PE parsing & import resolution
│   │   │   │   └── pe_parser.h
│   │   │   ├── proc/
│   │   │   │   ├── procmgr.cpp       # Process management
│   │   │   │   └── procmgr.h
│   │   │   ├── shell/
│   │   │   │   ├── shell_inj.cpp     # Main injection routine
│   │   │   │   └── shell_inj.h
│   │   │   └── stub/
│   │   │       ├── inj_stub.cpp      # Loader stub
│   │   │       └── inj_stub.h
│   │   ├── payload/
│   │   │   ├── shellcode.cpp         # DLL payload
│   │   │   └── shellcode.h
│   │   └── validation/
│   │       ├── event_mgr.cpp         # Event management
│   │       ├── event_mgr.h
│   │       ├── key_validator.cpp     # Key validation
│   │       └── key_validator.h
│   ├── entrypoint.cpp                # Driver entry point
│   ├── KRNLLiterally.vcxproj         # Visual Studio project
│   └── KRNLLiterally.vcxproj.filters # Project filters
├── LICENSE
└── README.md
```

---

## Injection Flow

```
1. Driver Loaded
   ↓
2. Driver Entrypoint (entrypoint.cpp)
   ↓
3. Background Thread Created
   ↓
4. Driver Start (driver_start)
   ↓
5. Await Process (AwaitProcess)
   └── Polls for mspaint.exe (target_process_name)
   └── Plays beeps when found
   ↓
6. Perform Injection (perform_injection)
   ├── Find target process (process_manager_find_by_name)
   ├── Open process handle (ObOpenObjectByPointer)
   ├── Map DLL (pe_loader_map_dll)
   │   ├── Allocate memory in target (memory_allocator_allocate)
   │   ├── Write PE headers
   │   ├── Map sections
   │   ├── Resolve imports
   │   │   ├── Find module base (get_module_base_in_target)
   │   │   ├── Resolve export by name (get_export_address)
   │   │   └── Write IAT entries
   │   └── Set entry point
   ├── Allocate loader stub
   ├── Prepare LOADER_DATA (entry_point + image_base)
   ├── Create remote thread (RtlCreateUserThread)
   └── Thread executes stub → calls DLL entry point
   ↓
7. Injected DLL Creates Key Window
   ↓
8. User Enters Key
   ↓
9. DLL Sends Validation via IOCTL
   └── ioctl_validate_key (BOOLEAN result)
   ↓
10. Driver Validates Key
    ├── event_manager_set_validation
    └── KeSetEvent signals validation event
    ↓
11. Driver Continues (if valid) or Unloads (if invalid/timeout)
```

---

## Key Validation System

### How It Works

1. **Driver Initialization**
   - Creates a KEVENT (validation_event) in non-signaled state
   - Registers validation context with IOCTL handler

2. **Validation Process**
   - `validator_perform_validation()` called
   - Waits for event with 180-second timeout
   - If timeout → `status_key_validation_timeout`
   - If invalid → `status_key_validation_failed`
   - If valid → `STATUS_SUCCESS`

3. **User-Mode Communication**
   - Injected DLL sends BOOLEAN via `ioctl_validate_key` IOCTL
   - Driver sets validation result and signals event

4. **Timeout Handling**
   - Validation times out after 180 seconds
   - Driver logs "validation timed out" and continues unloading

### IOCTL Protocol

| Field | Type | Description |
|-------|------|-------------|
| `ioctl_validate_key` | CTL_CODE | Receives BOOLEAN validation result |
| Buffer | BOOLEAN | TRUE = valid, FALSE = invalid |

---

## Building

### Prerequisites

- **Windows 10/11** (x64)
- **Visual Studio 2022** with:
  - Desktop development with C++
  - Windows Driver Kit (WDK) extension
- **Windows SDK** (latest)

### Build Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/imp1338/KRNL.git
   cd KRNL/KRNLLiterally
   ```

2. **Open in Visual Studio**
   - Open `KRNLLiterally.vcxproj`

3. **Build the driver**
   - Set configuration: `Release` / `x64`
   - Build → Build Solution
   - Output: `krnl.sys` in `build/` directory

### Build Output

- **Driver**: `build/krnl.sys`
- **Entry Point**: `entrypoint`
- **Target Name**: `krnl`

---

## 💻 Usage

> **DISCLAIMER**: This is a **proof-of-concept** for educational research. Use only in controlled, isolated environments (VMs). Misuse can crash the system.

### 1. Enable Test Mode (for unsigned drivers)

```cmd
bcdedit /set testsigning on
bcdedit /set nointegritychecks on
```

### 2. Load the Driver

```cmd
sc create krnl type= kernel binPath= C:\path\to\krnl.sys
sc start krnl
```

### 3. Observe Driver Logs

Use DebugView or WinDbg to view driver output:

```
[KRNL] background thread created, driver loaded
[KRNL] waiting for process: mspaint.exe
[KRNL] found process: mspaint.exe at 0x...
[KRNL] attempting injection...
[KRNL] allocated memory at: 0x... (size: ... bytes)
[KRNL] dll mapped at: 0x...
[KRNL] remote thread created (TID: ...)
[KRNL] validation started...
```

### 4. Key Validation

The injected DLL will:
1. Create a validation window
2. Wait for key input
3. Send validation result via IOCTL

### 5. Unload the Driver

```cmd
sc stop krnl
sc delete krnl
```

---

## Security Notes

- **Driver signing is required** for production use (test mode enabled for development)
- The key system provides **simple authentication** — not production-grade
- All injection is **POC quality** — not production-ready
- Use **only in VM environments** for testing

---

## Component Dependencies

```
entrypoint.cpp
    ├── driver.h
    │   └── types.h
    ├── ioctl.h
    │   └── types.h
    └── logging.h
        └── definitions.h

driver.cpp
    ├── driver.h
    ├── device.h
    ├── ioctl.h
    ├── key_validator.h
    │   └── event_mgr.h
    ├── shell_inj.h
    │   ├── types.h
    │   └── definitions.h
    └── logging.h

shell_inj.cpp
    ├── shell_inj.h
    ├── procmgr.h
    ├── pe_parser.h
    ├── inj_stub.h
    ├── memalloc.h
    ├── shellcode.h
    └── logging.h

pe_parser.cpp
    ├── pe_parser.h
    ├── memalloc.h
    └── logging.h
```

---

## Testing

### Driver Log Output

```
[KRNL] device name: \Device\krnl
[KRNL] creating device object...
[KRNL] IoCreateDevice returned status: 0x0
[KRNL] [success] device object created successfully
[KRNL] creating symbolic link...
[KRNL] [success] symbolic link created successfully
[KRNL] initializing key validator...
[KRNL] initializing validation event...
[KRNL] [success] validation event initialized
[KRNL] [success] key validator initialized
[KRNL] [success] driver initialization completed
[KRNL] background thread started
[KRNL] waiting for process: mspaint.exe
[KRNL] found process: mspaint.exe at 0xFFFF...
[KRNL] [success] process found: mspaint.exe
[KRNL] attempting injection...
[KRNL] starting dll injection into: mspaint.exe
[KRNL] mapping dll: size = 0x4800
[KRNL] allocated memory at: 0x... (size: ... bytes)
[KRNL] wrote pe headers
[KRNL] mapping section 0: VA=0x1000 Size=0x1000
[KRNL] [success] all sections mapped successfully
[KRNL] resolving imports for: kernel32.dll
[KRNL] resolving imports for: user32.dll
[KRNL] [success] import resolution complete
[KRNL] entry point at: 0x...
[KRNL] [success] dll mapped at: 0x...
[KRNL] loader stub at: 0x...
[KRNL] creating remote thread at ... with loader data ...
[KRNL] [success] remote thread created (TID: ...)
[KRNL] [success] injection completed
```

---

## License

This project is licensed under the **Apache License** – see the [LICENSE](LICENSE) file for details.

---

## Important Notes

- **POC only** – not for production use
- **Requires administrative privileges**
- **Driver signing required** (test mode can be enabled)
- **Use in VM** – kernel crashes can corrupt the system
- **Educational purpose** – understand kernel-mode injection techniques

---

## Acknowledgments

- Windows Driver Kit (WDK) documentation
- Windows Internals books and resources
- The security research community

---

## 📬 Contact

- **Author**: [imp1338](https://github.com/imp1338)
- **Issues**: [GitHub Issues](https://github.com/imp1338/KRNL/issues)

---

*Built for educational and research purposes only.*
