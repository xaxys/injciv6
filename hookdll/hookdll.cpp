#include <windows.h>
#include "inlinehook.h"
#include "fkhook.h"

#ifdef _MSC_VER
#pragma comment (lib, "ws2_32.lib")
#endif

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
