#include "ks_service.h"
#include "../common/definitions.h"
#include "../common/logger.h"
#include "../communication/ioctl.h"
#include <iostream>
#include <string>

bool validate_key(const std::string& key) {
    if (key.empty()) {
        return false;
    }

    return (key == correct_key);
}

void run_key_service() {
    std::string input_key;

    log_info("KRNL Successfully Initalized...");
    log_info("we are currently operaitng inside of kernail morde");

    printf("\n");
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("enter license key: ");
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

    std::getline(std::cin, input_key);

    printf("\n");

    log_info("validating key: %s", input_key.c_str());

    bool is_valid = validate_key(input_key);

    if (is_valid) {
        log_info("key validation successful!");
        log_info("requesting a callback of validation");

        if (send_validation_to_driver(true)) {
            log_info("validation complete");
        }
        else {
            log_error("failed to communicate with driver");
        }
    }
    else {
        log_error("invalid license key");
        log_error("sending failure to kernel driver...");

        send_validation_to_driver(false);
    }

    printf("\n");
    log_info("press enter to exit...");
    std::cin.get();
}