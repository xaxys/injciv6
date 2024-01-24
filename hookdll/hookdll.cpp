#include <windows.h>
#include <mutex>
#include "inlinehook.h"
#include "fkhook.h"

HANDLE hMutex = NULL;

BOOL APIENTRY DllMain(HINSTANCE hinstdll, DWORD reason, LPVOID reserved)
{
    switch (reason) {
        case DLL_PROCESS_ATTACH:
        {
            hMutex = CreateMutexA(NULL, FALSE, "fkhook");
            if (GetLastError() == ERROR_ALREADY_EXISTS) {
                CloseHandle(hMutex);
                return TRUE;
            }
            hook();
            break;
        }
        case DLL_PROCESS_DETACH:
        {
            if (hMutex != NULL) {
                CloseHandle(hMutex);
                hMutex = NULL;
            }
            unhook();
            break;
        }
    }
    return TRUE;
}
