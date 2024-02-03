#pragma once

#include <shellapi.h>
#include "inject.h"

inline bool runas_admin(const char *exename, const char* args = NULL)
{
    SHELLEXECUTEINFOA sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_FLAG_DDEWAIT | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = "runas";
    sei.lpFile = exename;
    sei.nShow = SW_SHOWNORMAL;
    sei.lpParameters = args;
    return ShellExecuteExA(&sei);
}

inline DWORD get_bg3_proc()
{
    DWORD pid = find_pid_by_name("bg3.exe");
    if (pid == 0)
        pid = find_pid_by_name("bg3_dx11.exe");
    return pid;
}
