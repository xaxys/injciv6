#include <windows.h>
#include "inlinehook.h"
#include "fkhook.h"

BOOL APIENTRY DllMain(HINSTANCE hinstdll, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        hook();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        unhook();
    }
    return TRUE;
}
