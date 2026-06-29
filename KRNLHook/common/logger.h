#pragma once
#include <windows.h>
#include <stdio.h>

enum class log_level {info,warning,error};

inline void set_console_color(log_level level) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    switch (level) {
    case log_level::info:
        SetConsoleTextAttribute(console, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        break;
    case log_level::warning:
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
        break;
    case log_level::error:
        SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
        break;
    }
}

inline void reset_console_color() {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

inline void log(log_level level, const char* format, ...) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(console, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("[ KRNL ] ");

    set_console_color(level);
    switch (level) {
    case log_level::info:    printf("INFO    "); break;
    case log_level::warning: printf("WARNING "); break;
    case log_level::error:   printf("ERROR   "); break;
    }

    SetConsoleTextAttribute(console, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("-> ");

    reset_console_color();

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

#define log_info(fmt, ...)    log(log_level::info, fmt, __VA_ARGS__)
#define log_warning(fmt, ...) log(log_level::warning, fmt, __VA_ARGS__)
#define log_error(fmt, ...)   log(log_level::error, fmt, __VA_ARGS__)