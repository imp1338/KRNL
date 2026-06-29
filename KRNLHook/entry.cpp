#include <windows.h>
#include <thread>
#include "common/logger.h"
#include "key_handler/ks_service.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONIN$", "r", stdin);

        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_FONT_INFOEX font = { sizeof(CONSOLE_FONT_INFOEX) };
        wcscpy_s(font.FaceName, L"Consolas");
        font.dwFontSize.Y = 13;
        font.FontWeight = FW_NORMAL;
        SetCurrentConsoleFontEx(console, FALSE, &font);
        SetConsoleTitleA("KRNL Key System");

        std::thread([]() {
            run_key_service();
            }).detach();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        fclose(stdout);
        fclose(stdin);
        FreeConsole();
    }

    return TRUE;
}