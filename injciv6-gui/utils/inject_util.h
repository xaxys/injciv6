#pragma once

#include <stdbool.h>
#include <windows.h>

bool runas_admin(const char *exename);
DWORD get_civ6_dx11_proc();
DWORD get_civ6_dx12_proc();
DWORD get_civ6_proc();
bool get_proc_path(DWORD pid, LPWSTR buf, DWORD bufsize);
bool get_civ6_path(LPWSTR buf, DWORD bufsize);
bool is_injciv6_running(DWORD pid);
