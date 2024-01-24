#pragma once
#include <stdbool.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

bool inject_dll(DWORD pid, const char *dll_path);
DWORD find_pid_by_name(const char *name);
HMODULE find_module_handle_from_pid(DWORD pid, const char *module_name);
bool remove_module(DWORD pid, HMODULE module_handle);

#ifdef __cplusplus
}
#endif
