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

inline DWORD get_civ6_proc()
{
    DWORD pid = find_pid_by_name("CivilizationVI.exe");
    if (pid == 0)
        pid = find_pid_by_name("CivilizationVI_DX12.exe");
    return pid;
}
