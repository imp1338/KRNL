#include "ioctl.h"
#include "../common/definitions.h"
#include "../common/logger.h"

bool send_validation_to_driver(bool is_valid) {
    log_info("attempting to open device: %s", driver_symlink);

    HANDLE device = CreateFileA(driver_symlink,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (device == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();

        switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            log_error("driver device not found - is the driver loaded?");
            break;
        case ERROR_ACCESS_DENIED:
            log_error("access denied - run as administrator");
            break;
        case ERROR_DRIVER_FAILED_PRIOR_UNLOAD:
            log_error("driver failed to unload properly - reboot may be required");
            break;
        default:
            log_error("failed to open driver device (error: %d)", error);
            break;
        }

        return false;
    }

    DWORD bytes_returned;
    bool result = DeviceIoControl(device,ioctl_validate_key,&is_valid,sizeof(bool),NULL,0,&bytes_returned,NULL);
    CloseHandle(device);

    if (!result) {
        log_error("failed to send ioctl (error: %d)", GetLastError());
        return false;
    }

    log_info("validation sent to driver successfully");
    return true;
}